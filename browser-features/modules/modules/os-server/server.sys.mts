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
} from "./router.sys.mts";

import type { HealthResponse } from "./api-spec/types.ts";

// Route handlers from separated modules
import { registerBrowserRoutes } from "./browser/routes.sys.mts";
import { registerScraperRoutes } from "./scraper/routes.sys.mts";
import { registerTabRoutes } from "./tabs/routes.sys.mts";

// -- Timer import -------------------------------------------------------------

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// -- Constants ----------------------------------------------------------------

const DEFAULT_PORT = 58261;
const PREF_ENABLED = "floorp.os.enabled";
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
  private static readonly READ_HEAD_TIMEOUT_MS = 5000;
  private static readonly READ_BODY_TIMEOUT_MS = 8000;
  private static readonly MAX_BODY_BYTES = 2 * 1024 * 1024; // 2MB

  QueryInterface = ChromeUtils.generateQI([
    "nsIServerSocketListener",
    "nsIObserver",
  ]);

  start(port: number, token: string) {
    if (this._server) return; // already started
    this._token = token || "";

    const server = Cc["@mozilla.org/network/server-socket;1"].createInstance(
      Ci.nsIServerSocket,
    );
    // loopbackOnly=true, backlog=-1 default
    server.init(port, /* loopbackOnly */ true, -1);
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
      prefName === PREF_PORT ||
      prefName === PREF_TOKEN
    ) {
      this._updateFromPrefs();
    }
  }

  public registerPrefObserver() {
    try {
      Services.prefs.addObserver(PREF_ENABLED, this);
      Services.prefs.addObserver(PREF_PORT, this);
      Services.prefs.addObserver(PREF_TOKEN, this);
    } catch (e) {
      void e; // ignore
    }
  }

  private _unregisterPrefObserver() {
    try {
      Services.prefs.removeObserver(PREF_ENABLED, this);
      Services.prefs.removeObserver(PREF_PORT, this);
      Services.prefs.removeObserver(PREF_TOKEN, this);
    } catch (e) {
      void e; // ignore
    }
  }

  private _updateFromPrefs() {
    const enabled = Services.prefs.getBoolPref(PREF_ENABLED, false);
    if (enabled) {
      const port = Services.prefs.getIntPref(PREF_PORT, DEFAULT_PORT);
      const token = Services.prefs.getStringPref(PREF_TOKEN, "");
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
    const handle = async () => {
      try {
        const input = transport.openInputStream(0, 0, 0);
        const output = transport.openOutputStream(0, 0, 0);
        const sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
          Ci.nsIScriptableInputStream,
        );
        sis.init(input);

        // Read until we hit CRLF CRLF for headers
        const chunks: string[] = [];
        let head = "";
        const headDeadline = Date.now() + LocalHttpServer.READ_HEAD_TIMEOUT_MS;
        while (true) {
          const avail = sis.available();
          if (avail > 0) {
            chunks.push(sis.read(avail));
            head = chunks.join("");
            if (head.includes("\r\n\r\n")) break;
          } else {
            if (Date.now() > headDeadline) {
              const res = requestTimeout();
              writeUtf8(output, res);
              output.close();
              input.close();
              return;
            }
            await new Promise<void>((resolve) =>
              setTimeout(() => resolve(), 5),
            );
          }
        }
        const split = head.indexOf("\r\n\r\n");
        const headStr = split >= 0 ? head.slice(0, split) : head;
        const req = parseRequestHead(headStr);
        if (!req) {
          {
            const res = jsonResponse(400, { error: "malformed request" });
            writeUtf8(output, res);
          }
          output.close();
          input.close();
          return;
        }
        // Determine body length
        const contentLength =
          Number.parseInt(req.headers["content-length"] || "0", 10) || 0;
        if (contentLength < 0) {
          const res = badRequest("invalid content-length");
          writeUtf8(output, res);
          output.close();
          input.close();
          return;
        }
        if (contentLength > LocalHttpServer.MAX_BODY_BYTES) {
          const res = payloadTooLarge();
          writeUtf8(output, res);
          output.close();
          input.close();
          return;
        }
        const leftover = split >= 0 ? head.slice(split + 4) : "";
        const bodyBytes: number[] = binaryStringToByteArray(leftover);
        const bodyDeadline = Date.now() + LocalHttpServer.READ_BODY_TIMEOUT_MS;
        while (bodyBytes.length < contentLength) {
          const avail = sis.available();
          if (avail > 0) {
            const piece = sis.read(avail);
            bodyBytes.push(...binaryStringToByteArray(piece));
          } else {
            if (Date.now() > bodyDeadline) {
              const res = badRequest("incomplete body");
              writeUtf8(output, res);
              output.close();
              input.close();
              return;
            }
            await new Promise<void>((resolve) =>
              setTimeout(() => resolve(), 5),
            );
          }
        }
        req.body = new Uint8Array(bodyBytes.slice(0, contentLength));

        // Auth (optional)
        if (this._token) {
          const auth = req.headers["authorization"] || "";
          const expect = `Bearer ${this._token}`;
          if (auth !== expect) {
            {
              const res = unauthorized();
              writeUtf8(output, res);
            }
            output.close();
            input.close();
            return;
          }
        }

        // Route (async)
        const response = await this.routeAsync(req);
        writeUtf8(output, response);
        output.close();
        input.close();
      } catch (e) {
        try {
          const output = transport.openOutputStream(0, 0, 0);
          const res = serverError(String(e));
          writeUtf8(output, res);
          output.close();
        } catch {
          // ignore
        }
        err("socket error: ", e);
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

    return router;
  }

  private async routeAsync(req: HttpRequest): Promise<string> {
    const { method, path } = req;
    if (method === "OPTIONS") return corsPreflight();

    const { pathname, searchParams } = parseURL(path);
    const router = this._router;
    if (!router) return serverError("router not initialized");

    const match = router.match(method, pathname);
    if (!match) return notFound();

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
      const result = await match.handler(ctx);
      const status = result.status ?? 200;
      return jsonResponse(status, result.body ?? {});
    } catch (e) {
      const msg = String(e ?? "");
      // Map common service errors to 404 when instance is missing
      if (/instance\s+not\s+found/i.test(msg)) {
        return notFound();
      }
      return serverError(msg);
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
    if (enabled) {
      const port = Services.prefs.getIntPref(PREF_PORT, DEFAULT_PORT);
      const token = Services.prefs.getStringPref(PREF_TOKEN, "");
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
  osLocalServer.start(port, token);
export const stopServer = () => osLocalServer.stop();
