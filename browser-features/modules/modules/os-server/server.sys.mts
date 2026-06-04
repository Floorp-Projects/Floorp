/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Floorp OS Local HTTP Server (localhost only)
 *
 * Purpose
 * - Expose a minimal subset of os-apis to other local applications via HTTP/JSON.
 * - Loopback-only. Optional bearer token auth (disabled by default).
 *
 * Endpoints (JSON)
 * - GET  /health                                 -> { status: "ok" }
 * - GET  /browser/tabs                           -> Tab[]
 * - GET  /browser/history?limit=10               -> HistoryItem[]
 * - GET  /browser/downloads?limit=10             -> Download[]
 * - GET  /browser/context?historyLimit=10&downloadLimit=10
 *                                               -> { history, tabs, downloads }
 * - OPTIONS /*                                   -> CORS preflight
 */

// -- Imports -----------------------------------------------------------------

import type { HttpRequest } from "./http/types.ts";
import {
  badRequest,
  binaryStringToByteArray,
  corsPreflight,
  err,
  getJSON,
  jsonResponse,
  log,
  logPerf,
  notFound,
  parseRequestHead,
  parseURL,
  payloadTooLarge,
  requestTimeout,
  serverError,
  unauthorized,
  writeUtf8,
} from "./http/utils.sys.mts";

import {
  type Context as RouterContext,
  createApi,
  type HttpMethod,
  Router as _Router,
  type StreamResult,
} from "./router.sys.mts";

import type { HealthResponse } from "./_os-plugin/api-spec/types.ts";

// Route handlers from separated modules
import { registerBrowserRoutes } from "./browser/routes.sys.mts";
import { registerScraperRoutes } from "./scraper/routes.sys.mts";
import { registerTabRoutes } from "./tabs/routes.sys.mts";
import { registerWorkspaceRoutes } from "./workspaces/routes.sys.mts";

// -- Timer import -------------------------------------------------------------

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// -- Constants ----------------------------------------------------------------

const DEFAULT_PORT = 58261;
const PREF_ENABLED = "floorp.os.enabled";
const PREF_MCP_ENABLED = "floorp.mcp.enabled";
const PREF_PORT = "floorp.os.server.port";
const PREF_TOKEN = "floorp.os.server.token";

// -- HTTP method helper -------------------------------------------------------

function asHttpMethod(m: string): HttpMethod {
  switch (m) {
    case "GET":
    case "POST":
    case "DELETE":
    case "OPTIONS":
      return m;
    default:
      return "GET";
  }
}

// -- Server ------------------------------------------------------------------

class LocalHttpServer implements nsIServerSocketListener {
  private _server: nsIServerSocket | null = null;
  private _token = "";
  private _router: _Router | null = null;
  private _activeConnections = 0;
  private _updatingFromPrefs = false;
  private static readonly READ_HEAD_TIMEOUT_MS = 5000;
  private static readonly READ_BODY_TIMEOUT_MS = 8000;
  private static readonly MAX_BODY_BYTES = 2 * 1024 * 1024; // 2MB
  private static readonly MAX_CONCURRENT_CONNECTIONS = 50;

  QueryInterface = ChromeUtils.generateQI([
    "nsIServerSocketListener",
    "nsIObserver",
  ]);

  /**
   * Wait for data to become available on an nsIInputStream using the
   * native asyncWait() notification instead of setTimeout-based polling.
   *
   * On Windows, each setTimeout creates an IOCP (I/O Completion Port) packet
   * in the kernel.  Busy-polling at short intervals (e.g. 5ms) accumulates
   * millions of unreaped IOCP packets, each consuming ~80 bytes of kernel
   * nonpaged pool memory — leading to multi-gigabyte kernel memory leaks
   * (Floorp issue #2379).
   *
   * asyncWait() uses the platform's native I/O event notification and
   * produces zero polling overhead.
   *
   * @returns `true` if data became available, `false` on timeout.
   */
  private static waitForInput(
    inputStream: nsIInputStream,
    timeoutMs: number,
  ): Promise<boolean> {
    return new Promise<boolean>((resolve) => {
      let settled = false;

      const listener = {
        QueryInterface: ChromeUtils.generateQI(["nsIInputStreamCallback"]),
        onInputStreamReady(_stream: nsIAsyncInputStream) {
          if (!settled) {
            settled = true;
            clearTimeout(timer);
            resolve(true);
          }
        },
      };

      // Cast to nsIAsyncInputStream — the stream returned by
      // nsISocketTransport.openInputStream() always implements it.
      const asyncStream = inputStream as unknown as nsIAsyncInputStream;
      asyncStream.asyncWait(listener, 0, 0, Services.tm.mainThread);

      // Safety timeout — ensures we don't hang forever if the native
      // notification never fires (e.g. socket closed mid-read)
      const timer = setTimeout(() => {
        if (!settled) {
          settled = true;
          // Cancel the asyncWait listener so it doesn't fire on a stale stream
          try {
            asyncStream.asyncWait(
              null as unknown as nsIInputStreamCallback,
              0,
              0,
              Services.tm.mainThread,
            );
          } catch {
            /* ignore */
          }
          resolve(false);
        }
      }, timeoutMs);
    });
  }

  start(port: number, token: string) {
    if (this._server) return; // already started
    this._token = token || "";

    const server = Cc["@mozilla.org/network/server-socket;1"].createInstance(
      Ci.nsIServerSocket,
    );
    try {
      // loopbackOnly=true, backlog=10 (max pending connections)
      // Using explicit backlog value instead of -1 which can cause NS_ERROR_CONNECTION_REFUSED on some systems
      server.init(port, /* loopbackOnly */ true, 10);
    } catch (e: unknown) {
      // Log all exception details for debugging
      if (e instanceof Ci.nsIException) {
        err(`Failed to start server on port ${port}`);
        err(`Error result: 0x${e.result.toString(16)} (${e.result})`);
        err(`Error message: ${e.message}`);
        err(`Error name: ${e.name}`);

        // Known error codes for port issues
        const isPortError =
          e.result === Cr.NS_ERROR_CONNECTION_REFUSED ||
          e.result === Cr.NS_ERROR_SOCKET_ADDRESS_IN_USE ||
          e.result === Cr.NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED ||
          e.result === Cr.NS_ERROR_NET_RESET ||
          e.result === 0x804b000d; // NS_ERROR_CONNECTION_REFUSED raw value

        if (isPortError) {
          err(
            `Please try: 1) Change 'floorp.os.server.port' in about:config to another port (e.g., 58262), 2) Restart Floorp, or 3) Check your firewall settings`,
          );
          return;
        }
      }
      // Log unknown errors
      err(`Unexpected error starting server: ${e}`);
      // Re-throw unexpected errors
      throw e;
    }
    server.asyncListen(this);
    this._server = server;
    log(`listening on 127.0.0.1:${server.port}`);

    // Build router once when starting
    this._router = this.buildRouter();

    // Ensure we stop on shutdown
    Services.obs.addObserver(this, "xpcom-shutdown");
  }

  stop() {
    if (!this._server) return;
    try {
      this._server.close();
    } catch (e) {
      void e; // ignore close errors
    }
    this._server = null;
    Services.obs.removeObserver(this, "xpcom-shutdown");
    log("stopped");
  }

  // nsIObserver
  observe(_subject: unknown, topic: string, _data?: string) {
    if (topic === "xpcom-shutdown") {
      this.stop();
      this._unregisterPrefObserver();
    } else if (topic === "nsPref:changed") {
      this._handlePrefChange(_data || "");
    }
  }

  private _handlePrefChange(prefName: string) {
    if (
      prefName === PREF_ENABLED ||
      prefName === PREF_MCP_ENABLED ||
      prefName === PREF_PORT ||
      prefName === PREF_TOKEN
    ) {
      this._updateFromPrefs();
    }
  }

  public registerPrefObserver() {
    try {
      Services.prefs.addObserver(PREF_ENABLED, this);
      Services.prefs.addObserver(PREF_MCP_ENABLED, this);
      Services.prefs.addObserver(PREF_PORT, this);
      Services.prefs.addObserver(PREF_TOKEN, this);
    } catch (e) {
      void e; // ignore
    }
  }

  private _unregisterPrefObserver() {
    try {
      Services.prefs.removeObserver(PREF_ENABLED, this);
      Services.prefs.removeObserver(PREF_MCP_ENABLED, this);
      Services.prefs.removeObserver(PREF_PORT, this);
      Services.prefs.removeObserver(PREF_TOKEN, this);
    } catch (e) {
      void e; // ignore
    }
  }

  public ensureToken(): string {
    const existing = Services.prefs.getStringPref(PREF_TOKEN, "");
    if (existing) {
      this.writeTokenFile(existing);
      return existing;
    }
    const generated = Services.uuid.generateUUID().toString()
      .replace(/[{}]/g, "");
    try {
      Services.prefs.setStringPref(PREF_TOKEN, generated);
    } catch (e) {
      err("Failed to persist server token to preferences:", e);
    }
    this.writeTokenFile(generated);
    return generated;
  }

  private writeTokenFile(token: string): void {
    try {
      const homeDir = Services.dirsvc.get("Home", Ci.nsIFile).path;
      const floorpDir = PathUtils.join(homeDir, ".floorp");
      const tokenFile = PathUtils.join(floorpDir, "os-server-token");
      const encoder = new TextEncoder();
      const content = encoder.encode(token);
      void IOUtils.makeDirectory(floorpDir, { ignoreExisting: true }).then(
        () => IOUtils.write(tokenFile, content, { mode: "overwrite" }),
      ).then(
        () => IOUtils.setPermissions(tokenFile, 0o600),
      ).catch(
        (e: unknown) => {
          err("Failed to write token file:", e);
        },
      );
    } catch (e) {
      err("Failed to write token file:", e);
    }
  }

  public deleteTokenFile(): void {
    try {
      const homeDir = Services.dirsvc.get("Home", Ci.nsIFile).path;
      const tokenFile = PathUtils.join(homeDir, ".floorp", "os-server-token");
      void IOUtils.remove(tokenFile, { ignoreAbsent: true }).catch(
        (e: unknown) => {
          err("Failed to delete token file:", e);
        },
      );
    } catch (e) {
      err("Failed to delete token file:", e);
    }
  }

  private _updateFromPrefs() {
    if (this._updatingFromPrefs) return;
    this._updatingFromPrefs = true;
    try {
      this._updateFromPrefsInner();
    } finally {
      this._updatingFromPrefs = false;
    }
  }

  private _updateFromPrefsInner() {
    const enabled = Services.prefs.getBoolPref(PREF_ENABLED, false);
    const mcpEnabled = Services.prefs.getBoolPref(PREF_MCP_ENABLED, false);
    if (enabled || mcpEnabled) {
      const port = Services.prefs.getIntPref(PREF_PORT, DEFAULT_PORT);
      const token = this.ensureToken();
      if (!this._server) {
        this.start(port, token);
      } else {
        // If port or token changed, restart
        const currentPort = this._server.port;
        if (currentPort !== port || this._token !== token) {
          this.stop();
          this.start(port, token);
        }
      }
    } else {
      if (this._server) {
        this.stop();
      }
    }
  }

  // nsIServerSocketListener
  onSocketAccepted(_serv: nsIServerSocket, transport: nsISocketTransport) {
    // Connection limit check to prevent resource exhaustion
    if (this._activeConnections >= LocalHttpServer.MAX_CONCURRENT_CONNECTIONS) {
      try {
        transport.close(Cr.NS_OK);
      } catch {
        /* ignore */
      }
      return;
    }
    this._activeConnections++;

    const handle = async () => {
      // Keep nullable handles for finally cleanup — streams are opened inside
      // try so that any openInputStream/openOutputStream failure still runs
      // finally and correctly decrements _activeConnections.
      let cleanupInput: nsIInputStream | null = null;
      let cleanupOutput: nsIOutputStream | null = null;
      let cleanupTransport: nsISocketTransport | null = transport;
      try {
        const input = transport.openInputStream(0, 0, 0);
        const output = transport.openOutputStream(0, 0, 0);
        cleanupInput = input;
        cleanupOutput = output;

        const sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
          Ci.nsIScriptableInputStream,
        );
        sis.init(input);

        // Read until we hit CRLF CRLF for headers
        // Use string concatenation (rope-based O(1) amortized in SpiderMonkey)
        // instead of chunks.join("") per read which copies the entire string each time.
        let head = "";
        const headDeadline = Date.now() + LocalHttpServer.READ_HEAD_TIMEOUT_MS;
        while (true) {
          const avail = sis.available();
          if (avail > 0) {
            head += sis.read(avail);
            if (head.includes("\r\n\r\n")) break;
          } else {
            const remaining = headDeadline - Date.now();
            if (remaining <= 0) {
              writeUtf8(output, requestTimeout());
              return;
            }
            // Use nsIInputStream.asyncWait() instead of setTimeout polling.
            // On Windows, each setTimeout creates an IOCP packet in the kernel;
            // busy-polling at 5ms intervals generates massive IOCP packet
            // accumulation that causes kernel nonpaged pool memory leaks.
            // asyncWait uses native socket event notification — zero polling.
            const dataReady = await LocalHttpServer.waitForInput(
              input,
              remaining,
            );
            if (!dataReady) {
              writeUtf8(output, requestTimeout());
              return;
            }
          }
        }
        const split = head.indexOf("\r\n\r\n");
        const headStr = split >= 0 ? head.slice(0, split) : head;
        const req = parseRequestHead(headStr);
        if (!req) {
          writeUtf8(output, jsonResponse(400, { error: "malformed request" }));
          return;
        }
        // Determine body length
        const contentLength =
          Number.parseInt(req.headers["content-length"] || "0", 10) || 0;
        if (contentLength < 0) {
          writeUtf8(output, badRequest("invalid content-length"));
          return;
        }
        if (contentLength > LocalHttpServer.MAX_BODY_BYTES) {
          writeUtf8(output, payloadTooLarge());
          return;
        }
        const leftover = split >= 0 ? head.slice(split + 4) : "";
        // Accumulate binary strings and convert once at end,
        // instead of push(...bytes) per chunk which is O(n²).
        const bodyParts: string[] = [leftover];
        let bodyLen = leftover.length;
        const bodyDeadline = Date.now() + LocalHttpServer.READ_BODY_TIMEOUT_MS;
        while (bodyLen < contentLength) {
          const avail = sis.available();
          if (avail > 0) {
            const piece = sis.read(avail);
            bodyParts.push(piece);
            bodyLen += piece.length;
          } else {
            const remaining = bodyDeadline - Date.now();
            if (remaining <= 0) {
              writeUtf8(output, badRequest("incomplete body"));
              return;
            }
            // Use asyncWait instead of setTimeout — same rationale as header loop
            const dataReady = await LocalHttpServer.waitForInput(
              input,
              remaining,
            );
            if (!dataReady) {
              writeUtf8(output, badRequest("incomplete body"));
              return;
            }
          }
        }
        const fullBody = bodyParts.join("");
        req.body = new Uint8Array(
          binaryStringToByteArray(fullBody).slice(0, contentLength),
        );

        // Auth (optional)
        if (this._token) {
          const auth = req.headers["authorization"] || "";
          const expect = `Bearer ${this._token}`;
          if (auth !== expect) {
            writeUtf8(output, unauthorized());
            return;
          }
        }

        // Route (async) — route handler is responsible for writing and closing streams
        await this.routeAsync(req, output, input);
        // Prevent finally from double-closing
        cleanupInput = null;
        cleanupOutput = null;
        cleanupTransport = null;
      } catch (e) {
        try {
          if (cleanupOutput) {
            writeUtf8(cleanupOutput, serverError(String(e)));
          }
        } catch {
          // ignore write failure on already-closed stream
        }
        err("socket error: ", e);
      } finally {
        this._activeConnections--;
        try {
          cleanupOutput?.close();
        } catch {
          /* ignore */
        }
        try {
          cleanupInput?.close();
        } catch {
          /* ignore */
        }
        try {
          cleanupTransport?.close(Cr.NS_OK);
        } catch {
          /* ignore */
        }
      }
    };
    // Fire and forget
    handle();
  }

  onStopListening(_serv: nsIServerSocket, _status: nsresult) {
    // no-op
  }

  private buildRouter(): _Router {
    const router = new _Router();
    const api = createApi(router);

    // Health endpoint
    api.get<undefined, HealthResponse>("/health", () => ({
      status: 200,
      body: { status: "ok" },
    }));

    // Register routes from separated modules
    registerBrowserRoutes(api);
    registerScraperRoutes(api);
    registerTabRoutes(api);
    registerWorkspaceRoutes(api);

    return router;
  }

  private async routeAsync(
    req: HttpRequest,
    output: nsIOutputStream,
    input: nsIInputStream,
  ): Promise<void> {
    const { method, path } = req;
    if (method === "OPTIONS") {
      writeUtf8(output, corsPreflight());
      output.close();
      input.close();
      return;
    }

    const { pathname, searchParams } = parseURL(path);
    const router = this._router;
    if (!router) {
      writeUtf8(output, serverError("router not initialized"));
      output.close();
      input.close();
      return;
    }

    const match = router.match(method, pathname);
    if (!match) {
      writeUtf8(output, notFound());
      output.close();
      input.close();
      return;
    }

    try {
      const jsonFn = (() => getJSON(req)) as unknown as RouterContext["json"];
      const ctx: RouterContext = {
        method: asHttpMethod(method),
        pathname,
        searchParams,
        headers: req.headers,
        body: req.body,
        params: match.params,
        json: jsonFn,
      };
      const _t0 = Date.now();
      const result = await match.handler(ctx);
      logPerf(`${method} ${pathname}`, Date.now() - _t0);

      // Stream handling
      if ("isStream" in result && result.isStream) {
        const streamResult = result as StreamResult;

        const headers =
          "HTTP/1.1 200 OK\r\n" +
          "Content-Type: text/event-stream\r\n" +
          "Cache-Control: no-cache\r\n" +
          "Connection: keep-alive\r\n" +
          "Access-Control-Allow-Origin: *\r\n" +
          "\r\n";
        writeUtf8(output, headers);

        // Callback to clean up — stream callbacks own the stream lifecycle
        streamResult.onConnect(
          (data) => {
            try {
              writeUtf8(output, `data: ${data}\n\n`);
            } catch {
              // Write failed, likely closed
              output.close();
              input.close();
            }
          },
          () => {
            try {
              output.close();
              input.close();
            } catch {
              // ignore
            }
          },
        );
        return;
      }

      // Normal JSON response
      const httpResult = result as { status?: number; body?: unknown };
      const status = httpResult.status ?? 200;
      const response = jsonResponse(status, httpResult.body ?? {});
      writeUtf8(output, response);
      output.close();
      input.close();
    } catch (e) {
      const msg = String(e ?? "");
      let response;
      // Map common service errors to 404 when instance/browser is missing
      if (/(?:instance|browser)\s+not\s+found/i.test(msg)) {
        response = notFound();
      } else {
        response = serverError(msg);
      }
      writeUtf8(output, response);
      output.close();
      input.close();
    }
  }
}

export const osLocalServer = new LocalHttpServer();

// Initialize server based on preferences
function initializeServer() {
  try {
    // Register pref observer
    osLocalServer.registerPrefObserver();

    // Check initial pref value
    const enabled = Services.prefs.getBoolPref(PREF_ENABLED, false);
    const mcpEnabled = Services.prefs.getBoolPref(PREF_MCP_ENABLED, false);
    if (enabled || mcpEnabled) {
      const port = Services.prefs.getIntPref(PREF_PORT, DEFAULT_PORT);
      const token = osLocalServer.ensureToken();
      osLocalServer.start(port, token);
    }
  } catch (e) {
    err("failed to initialize server:", e);
  }
}

// Initialize on module load
initializeServer();

// Also export explicit control API
export const startServer = (port = DEFAULT_PORT, token = "") =>
  osLocalServer.start(port, token || osLocalServer.ensureToken());
export const stopServer = () => osLocalServer.stop();
