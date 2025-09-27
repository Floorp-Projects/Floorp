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

// gecko built-ins are available globally: ChromeUtils, Services, Cc, Ci

type HeadersMap = Record<string, string>;

// Lazy import of BrowserInfo (read-only os-apis)
interface BrowserInfoAPI {
  getRecentTabs(): Promise<
    Array<{
      id: number;
      url: string;
      title: string;
      isActive: boolean;
      isPinned: boolean;
    }>
  >;
  getRecentHistory(limit?: number): Promise<
    Array<{
      url: string;
      title: string;
      lastVisitDate: number;
      visitCount: number;
    }>
  >;
  getRecentDownloads(limit?: number): Promise<
    Array<{
      id: number;
      url: string;
      filename: string;
      status: string;
      startTime: number;
    }>
  >;
  getAllContextData(
    historyLimit?: number,
    downloadLimit?: number,
  ): Promise<{
    history: Array<{
      url: string;
      title: string;
      lastVisitDate: number;
      visitCount: number;
    }>;
    tabs: Array<{
      id: number;
      url: string;
      title: string;
      isActive: boolean;
      isPinned: boolean;
    }>;
    downloads: Array<{
      id: number;
      url: string;
      filename: string;
      status: string;
      startTime: number;
    }>;
  }>;
}

const BrowserInfoModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/browser-info/BrowserInfo.sys.mjs",
  ) as { BrowserInfo: BrowserInfoAPI };

// Lazy import of WebScraper service (headless browsing)
interface WebScraperAPI {
  createInstance(): Promise<string>;
  destroyInstance(instanceId: string): void;
  navigate(instanceId: string, url: string): Promise<void>;
  getURI(instanceId: string): Promise<string | null>;
  getHTML(instanceId: string): Promise<string | null>;
  getElement(instanceId: string, selector: string): Promise<string | null>;
  getElementText(instanceId: string, selector: string): Promise<string | null>;
  clickElement(instanceId: string, selector: string): Promise<boolean>;
  waitForElement(
    instanceId: string,
    selector: string,
    timeout?: number,
  ): Promise<boolean>;
  executeScript(instanceId: string, script: string): Promise<void>;
  takeScreenshot(instanceId: string): Promise<string | null>;
  takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null>;
  takeFullPageScreenshot(instanceId: string): Promise<string | null>;
  takeRegionScreenshot(
    instanceId: string,
    rect?: { x?: number; y?: number; width?: number; height?: number },
  ): Promise<string | null>;
  fillForm(
    instanceId: string,
    formData: { [selector: string]: string },
  ): Promise<boolean>;
  getValue(instanceId: string, selector: string): Promise<string | null>;
  submit(instanceId: string, selector: string): Promise<boolean>;
}

const WebScraperModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/web/WebScraperServices.sys.mjs",
  ) as { WebScraper: WebScraperAPI };

// Lazy import of TabManager services (visible tab automation)
interface TabManagerAPI {
  createInstance(
    url: string,
    options?: { inBackground?: boolean },
  ): Promise<string>;
  attachToTab(browserId: string): Promise<string | null>;
  listTabs(): Promise<
    Array<{
      browserId: string;
      title: string;
      url: string;
      selected: boolean;
      pinned: boolean;
    }>
  >;
  getInstanceInfo(instanceId: string): Promise<unknown | null>;
  destroyInstance(instanceId: string): void;
  navigate(instanceId: string, url: string): Promise<void>;
  getURI(instanceId: string): Promise<string>;
  getHTML(instanceId: string): Promise<string | null>;
  getElement(instanceId: string, selector: string): Promise<string | null>;
  getElementText(instanceId: string, selector: string): Promise<string | null>;
  clickElement(instanceId: string, selector: string): Promise<boolean | null>;
  waitForElement(
    instanceId: string,
    selector: string,
    timeout?: number,
  ): Promise<boolean | null>;
  executeScript(instanceId: string, script: string): Promise<void>;
  takeScreenshot(instanceId: string): Promise<string | null>;
  takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null>;
  takeFullPageScreenshot(instanceId: string): Promise<string | null>;
  takeRegionScreenshot(
    instanceId: string,
    rect?: { x?: number; y?: number; width?: number; height?: number },
  ): Promise<string | null>;
  fillForm(
    instanceId: string,
    formData: { [selector: string]: string },
  ): Promise<boolean | null>;
  getValue(instanceId: string, selector: string): Promise<string | null>;
  submit(instanceId: string, selector: string): Promise<boolean | null>;
}

const TabManagerModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/web/TabManagerServices.sys.mjs",
  ) as { TabManagerServices: TabManagerAPI };

// -- Small utilities ----------------------------------------------------------

function log(...args: unknown[]) {
  console.log("[Floorp OS-Server]", ...args);
}
function _warn(...args: unknown[]) {
  console.warn("[Floorp OS-Server]", ...args);
}
function err(...args: unknown[]) {
  console.error("[Floorp OS-Server]", ...args);
}
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

const DEFAULT_PORT = 58261;

// -- HTTP primitives ----------------------------------------------------------

interface HttpRequest {
  method: string;
  path: string; // includes query string
  headers: HeadersMap;
  body: Uint8Array;
}

function parseHeaders(raw: string): HeadersMap {
  const out: HeadersMap = {};
  for (const line of raw.split("\r\n")) {
    const idx = line.indexOf(":");
    if (idx > 0) {
      const key = line.slice(0, idx).trim().toLowerCase();
      const val = line.slice(idx + 1).trim();
      out[key] = val;
    }
  }
  return out;
}

function parseRequestHead(head: string): HttpRequest | null {
  const [reqLine, ...headerLines] = head.split("\r\n");
  const parts = reqLine.split(" ");
  if (parts.length < 3) return null;
  const method = parts[0];
  const path = parts[1] || "/";
  const headers = parseHeaders(headerLines.join("\r\n"));
  return { method, path, headers, body: new Uint8Array() };
}

function getBodyText(req: HttpRequest): string {
  try {
    const dec = new TextDecoder();
    return dec.decode(req.body);
  } catch (_) {
    return "";
  }
}

// Convert a binary string (from nsIScriptableInputStream.read) to bytes
function binaryStringToByteArray(s: string): number[] {
  const out = new Array<number>(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
  return out;
}

function getJSON<T = unknown>(req: HttpRequest): T | null {
  const ct = (req.headers["content-type"] || "").toLowerCase();
  if (!ct.includes("application/json")) return null;
  const txt = getBodyText(req).trim();
  if (!txt) return null;
  try {
    return JSON.parse(txt) as T;
  } catch (_) {
    return null;
  }
}

function jsonResponse(
  status: number,
  bodyObj: unknown,
  extraHeaders: HeadersMap = {},
): string {
  const body = JSON.stringify(bodyObj);
  const enc = new TextEncoder();
  const bytes = enc.encode(body);
  const headers: HeadersMap = {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": String(bytes.length),
    Connection: "close",
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, DELETE, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
    ...extraHeaders,
  };
  const head = `HTTP/1.1 ${status} ${statusText(status)}\r\n` +
    Object.entries(headers)
      .map(([k, v]) => `${k}: ${v}`)
      .join("\r\n") +
    "\r\n\r\n";
  return head + body;
}

function statusText(code: number): string {
  switch (code) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 204:
      return "No Content";
    case 408:
      return "Request Timeout";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 413:
      return "Payload Too Large";
    case 500:
      return "Internal Server Error";
    default:
      return "";
  }
}

// Write UTF-8 bytes to an nsIOutputStream safely
function writeUtf8(out: nsIOutputStream, text: string) {
  const enc = new TextEncoder();
  const bytes = enc.encode(text);
  const bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream,
  );
  bos.setOutputStream(out);
  // nsIBinaryOutputStream#writeByteArray expects a u8[]; convert from Uint8Array
  bos.writeByteArray(Array.from(bytes));
}

function _badRequest(msg = "bad request") {
  return jsonResponse(400, { error: msg });
}
function requestTimeout(msg = "request timeout") {
  return jsonResponse(408, { error: msg });
}
function unauthorized(msg = "unauthorized") {
  return jsonResponse(401, { error: msg });
}
function notFound() {
  return jsonResponse(404, { error: "not found" });
}
function payloadTooLarge(msg = "payload too large") {
  return jsonResponse(413, { error: msg });
}
function serverError(msg = "internal error") {
  return jsonResponse(500, { error: msg });
}

// -- Router helpers -----------------------------------------------------------
import {
  type Context as RouterContext,
  createApi,
  type HttpMethod,
  type HttpResult as RouterHttpResult,
  type NamespaceBuilder,
  Router as _Router,
} from "./router.sys.mts";

// Import API types from OpenAPI specification
import type {
  Download,
  ErrorResponse,
  HealthResponse,
  HistoryItem,
  OkResponse,
  Tab,
} from "./api-spec/types.ts";

// Helper mappers to conform runtime data (numbers) to published API types (strings for some timestamps)
function mapHistoryItems(
  items: Array<
    { url: string; title: string; lastVisitDate: number; visitCount: number }
  >,
): HistoryItem[] {
  return items.map((h) => ({
    url: h.url,
    title: h.title,
    lastVisitDate: String(h.lastVisitDate),
    visitCount: h.visitCount,
  })) as HistoryItem[];
}
function mapDownloads(
  items: Array<
    {
      id: number;
      url: string;
      filename: string;
      status: string;
      startTime: number;
    }
  >,
): Download[] {
  return items.map((d) => ({
    id: d.id,
    url: d.url,
    filename: d.filename,
    status: d.status,
    startTime: String(d.startTime),
  })) as Download[];
}

function parseURL(path: string): {
  pathname: string;
  searchParams: URLSearchParams;
} {
  // Use a dummy origin to parse
  const u = new URL(`http://localhost${path}`);
  return { pathname: u.pathname, searchParams: u.searchParams };
}

function corsPreflight(): string {
  const headers: HeadersMap = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, DELETE, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
    "Content-Length": "0",
    Connection: "close",
  };
  const head = `HTTP/1.1 204 No Content\r\n` +
    Object.entries(headers)
      .map(([k, v]) => `${k}: ${v}`)
      .join("\r\n") +
    "\r\n\r\n";
  return head;
}

function clampInt(
  valueStr: string | null,
  def: number,
  min: number,
  max: number,
): number {
  const n = Number(valueStr ?? def);
  if (!Number.isFinite(n)) return def;
  return Math.max(min, Math.min(max, Math.floor(n)));
}

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
    } catch (_) {
      // ignore close errors
    }
    this._server = null;
    Services.obs.removeObserver(this, "xpcom-shutdown");
    log("stopped");
  }

  // nsIObserver
  observe(_subject: unknown, topic: string) {
    if (topic === "xpcom-shutdown") {
      this.stop();
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
              setTimeout(() => resolve(), 5)
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
          const res = _badRequest("invalid content-length");
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
              const res = _badRequest("incomplete body");
              writeUtf8(output, res);
              output.close();
              input.close();
              return;
            }
            await new Promise<void>((resolve) =>
              setTimeout(() => resolve(), 5)
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

    // Health
    api.get<undefined, HealthResponse>("/health", () => ({
      status: 200,
      body: { status: "ok" },
    }));

    // BrowserInfo
    api.namespace("/browser", (b: NamespaceBuilder) => {
      b.get<undefined, Tab[]>("/tabs", async () => {
        const { BrowserInfo } = BrowserInfoModule();
        const tabs = await BrowserInfo.getRecentTabs();
        return { status: 200, body: tabs } as RouterHttpResult<Tab[]>;
      });
      b.get<undefined, HistoryItem[]>(
        "/history",
        async (ctx: RouterContext<undefined>) => {
          const limit = clampInt(ctx.searchParams.get("limit"), 10, 0, 1000);
          const { BrowserInfo } = BrowserInfoModule();
          const historyRaw = await BrowserInfo.getRecentHistory(limit);
          const history = mapHistoryItems(historyRaw);
          return { status: 200, body: history };
        },
      );
      b.get<undefined, Download[]>(
        "/downloads",
        async (ctx: RouterContext<undefined>) => {
          const limit = clampInt(ctx.searchParams.get("limit"), 10, 0, 1000);
          const { BrowserInfo } = BrowserInfoModule();
          const downloadsRaw = await BrowserInfo.getRecentDownloads(limit);
          const downloads = mapDownloads(downloadsRaw);
          return { status: 200, body: downloads };
        },
      );
      b.get<
        undefined,
        { history: HistoryItem[]; tabs: Tab[]; downloads: Download[] }
      >("/context", async (ctx: RouterContext<undefined>) => {
        const h = clampInt(ctx.searchParams.get("historyLimit"), 10, 0, 1000);
        const d = clampInt(ctx.searchParams.get("downloadLimit"), 10, 0, 1000);
        const { BrowserInfo } = BrowserInfoModule();
        const out = await BrowserInfo.getAllContextData(h, d);
        const mapped = {
          history: mapHistoryItems(out.history),
          tabs: out.tabs,
          downloads: mapDownloads(out.downloads),
        };
        return { status: 200, body: mapped };
      });
    });

    // WebScraper
    api.namespace("/scraper", (s: NamespaceBuilder) => {
      s.get("/instances/:id/exists", async (ctx: RouterContext) => {
        const { WebScraper } = WebScraperModule();
        try {
          // Any call that touches the instance and throws when missing
          await WebScraper.getURI(ctx.params.id);
          return { status: 200, body: { exists: true } };
        } catch (e) {
          const msg = String(e ?? "");
          if (/instance\s+not\s+found/i.test(msg)) {
            return { status: 200, body: { exists: false } };
          }
          throw e;
        }
      });
      s.post<undefined, { instanceId: string }>("/instances", async () => {
        const { WebScraper } = WebScraperModule();
        const id = await WebScraper.createInstance();
        return { status: 200, body: { instanceId: id } };
      });
      s.delete("/instances/:id", (ctx: RouterContext) => {
        const id = ctx.params.id;
        const { WebScraper } = WebScraperModule();
        WebScraper.destroyInstance(id);
        return { status: 200, body: { ok: true } };
      });
      s.post<{ url: string }, OkResponse | ErrorResponse>(
        "/instances/:id/navigate",
        async (
          ctx: RouterContext<{ url: string }>,
        ) => {
          const json = ctx.json();
          if (!json?.url) {
            return { status: 400, body: { error: "url required" } };
          }
          const { WebScraper } = WebScraperModule();
          await WebScraper.navigate(ctx.params.id, json.url);
          return { status: 200, body: { ok: true } };
        },
      );
      s.get("/instances/:id/uri", async (ctx: RouterContext) => {
        const { WebScraper } = WebScraperModule();
        const uri = await WebScraper.getURI(ctx.params.id);
        return { status: 200, body: uri != null ? { uri } : {} };
      });
      s.get("/instances/:id/html", async (ctx: RouterContext) => {
        const { WebScraper } = WebScraperModule();
        const html = await WebScraper.getHTML(ctx.params.id);
        return { status: 200, body: html != null ? { html } : {} };
      });
      s.get("/instances/:id/elementText", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { WebScraper } = WebScraperModule();
        const text = await WebScraper.getElementText(ctx.params.id, sel);
        return { status: 200, body: text != null ? { text } : {} };
      });
      s.post("/instances/:id/click", async (ctx: RouterContext) => {
        const json = ctx.json() as { selector?: string } | null;
        const sel = json?.selector ?? "";
        const { WebScraper } = WebScraperModule();
        const okClicked = await WebScraper.clickElement(ctx.params.id, sel);
        return { status: 200, body: { ok: okClicked } };
      });
      s.post("/instances/:id/waitForElement", async (ctx: RouterContext) => {
        const json = ctx.json() as
          | { selector?: string; timeout?: number }
          | null;
        const sel = json?.selector ?? "";
        const to = json?.timeout ?? 5000;
        const { WebScraper } = WebScraperModule();
        const found = await WebScraper.waitForElement(ctx.params.id, sel, to);
        return { status: 200, body: { found } };
      });
      s.post<{ script: string }, OkResponse | ErrorResponse>(
        "/instances/:id/executeScript",
        async (ctx: RouterContext<{ script: string }>) => {
          const json = ctx.json();
          if (!json?.script) {
            return { status: 400, body: { error: "script required" } };
          }
          const { WebScraper } = WebScraperModule();
          await WebScraper.executeScript(ctx.params.id, json.script);
          return { status: 200, body: { ok: true } };
        },
      );
      // Screenshots
      s.get("/instances/:id/screenshot", async (ctx: RouterContext) => {
        const { WebScraper } = WebScraperModule();
        const image = await WebScraper.takeScreenshot(ctx.params.id);
        return { status: 200, body: image != null ? { image } : {} };
      });
      s.get("/instances/:id/elementScreenshot", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { WebScraper } = WebScraperModule();
        const image = await WebScraper.takeElementScreenshot(
          ctx.params.id,
          sel,
        );
        return { status: 200, body: image != null ? { image } : {} };
      });
      s.get("/instances/:id/fullPageScreenshot", async (ctx: RouterContext) => {
        const { WebScraper } = WebScraperModule();
        const image = await WebScraper.takeFullPageScreenshot(ctx.params.id);
        return { status: 200, body: image != null ? { image } : {} };
      });
      s.post("/instances/:id/regionScreenshot", async (ctx: RouterContext) => {
        const json = ctx.json() as {
          rect?: { x?: number; y?: number; width?: number; height?: number };
        } | null;
        const { WebScraper } = WebScraperModule();
        const image = await WebScraper.takeRegionScreenshot(
          ctx.params.id,
          json?.rect,
        );
        return { status: 200, body: image != null ? { image } : {} };
      });
      // Forms
      s.post("/instances/:id/fillForm", async (ctx: RouterContext) => {
        const json = ctx.json() as
          | { formData?: { [selector: string]: string } }
          | null;
        const { WebScraper } = WebScraperModule();
        const okFilled = await WebScraper.fillForm(
          ctx.params.id,
          json?.formData ?? {},
        );
        return { status: 200, body: { ok: okFilled } };
      });
      s.get("/instances/:id/value", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { WebScraper } = WebScraperModule();
        const value = await WebScraper.getValue(ctx.params.id, sel);
        return { status: 200, body: { value } };
      });
      s.post("/instances/:id/submit", async (ctx: RouterContext) => {
        const json = ctx.json() as { selector?: string } | null;
        const sel = json?.selector ?? "";
        const { WebScraper } = WebScraperModule();
        const submitted = await WebScraper.submit(ctx.params.id, sel);
        return { status: 200, body: { ok: submitted } };
      });
    });

    // TabManager
    api.namespace("/tabs", (t: NamespaceBuilder) => {
      t.get("/instances/:id/exists", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        const info = await TabManagerServices.getInstanceInfo(ctx.params.id);
        return { status: 200, body: { exists: info != null } };
      });
      t.get("/list", async () => {
        const { TabManagerServices } = TabManagerModule();
        const tabs = await TabManagerServices.listTabs();
        return { status: 200, body: tabs };
      });
      t.post<
        { url: string; inBackground?: boolean },
        { instanceId: string } | ErrorResponse
      >(
        "/instances",
        async (
          ctx: RouterContext<{ url: string; inBackground?: boolean }>,
        ) => {
          const json = ctx.json();
          if (!json?.url) {
            return { status: 400, body: { error: "url required" } };
          }
          const { TabManagerServices } = TabManagerModule();
          const id = await TabManagerServices.createInstance(json.url, {
            inBackground: json.inBackground ?? true,
          });
          return { status: 200, body: { instanceId: id } };
        },
      );
      t.post<
        { browserId: string },
        { instanceId: string | null } | ErrorResponse
      >(
        "/attach",
        async (ctx: RouterContext<{ browserId: string }>) => {
          const json = ctx.json();
          if (!json?.browserId) {
            return { status: 400, body: { error: "browserId required" } };
          }
          const { TabManagerServices } = TabManagerModule();
          const id = await TabManagerServices.attachToTab(json.browserId);
          return { status: 200, body: { instanceId: id } };
        },
      );
      t.get("/instances/:id", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        const info = await TabManagerServices.getInstanceInfo(ctx.params.id);
        return { status: 200, body: info };
      });
      t.delete("/instances/:id", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        await TabManagerServices.destroyInstance(ctx.params.id);
        return { status: 200, body: { ok: true } };
      });
      t.post<{ url: string }, OkResponse | ErrorResponse>(
        "/instances/:id/navigate",
        async (ctx: RouterContext<{ url: string }>) => {
          const json = ctx.json();
          if (!json?.url) {
            return { status: 400, body: { error: "url required" } };
          }
          const { TabManagerServices } = TabManagerModule();
          await TabManagerServices.navigate(ctx.params.id, json.url);
          return { status: 200, body: { ok: true } };
        },
      );
      t.get("/instances/:id/uri", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        const uri = await TabManagerServices.getURI(ctx.params.id);
        return { status: 200, body: { uri } };
      });
      t.get("/instances/:id/html", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        const html = await TabManagerServices.getHTML(ctx.params.id);
        return { status: 200, body: html != null ? { html } : {} };
      });
      t.get("/instances/:id/element", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { TabManagerServices } = TabManagerModule();
        const element = await TabManagerServices.getElement(
          ctx.params.id,
          sel,
        );
        return { status: 200, body: element != null ? { element } : {} };
      });
      t.get("/instances/:id/elementText", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { TabManagerServices } = TabManagerModule();
        const text = await TabManagerServices.getElementText(
          ctx.params.id,
          sel,
        );
        return { status: 200, body: text != null ? { text } : {} };
      });
      t.post("/instances/:id/click", async (ctx: RouterContext) => {
        const json = ctx.json() as { selector?: string } | null;
        const sel = json?.selector ?? "";
        const { TabManagerServices } = TabManagerModule();
        const okClicked = await TabManagerServices.clickElement(
          ctx.params.id,
          sel,
        );
        return { status: 200, body: { ok: okClicked } };
      });
      t.post("/instances/:id/waitForElement", async (ctx: RouterContext) => {
        const json = ctx.json() as
          | { selector?: string; timeout?: number }
          | null;
        const sel = json?.selector ?? "";
        const to = json?.timeout ?? 5000;
        const { TabManagerServices } = TabManagerModule();
        const found = await TabManagerServices.waitForElement(
          ctx.params.id,
          sel,
          to,
        );
        return { status: 200, body: { found } };
      });
      t.post<{ script: string }, OkResponse | ErrorResponse>(
        "/instances/:id/executeScript",
        async (ctx: RouterContext<{ script: string }>) => {
          const json = ctx.json();
          if (!json?.script) {
            return { status: 400, body: { error: "script required" } };
          }
          const { TabManagerServices } = TabManagerModule();
          await TabManagerServices.executeScript(ctx.params.id, json.script);
          return { status: 200, body: { ok: true } };
        },
      );
      t.get("/instances/:id/screenshot", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        const image = await TabManagerServices.takeScreenshot(ctx.params.id);
        return { status: 200, body: image != null ? { image } : {} };
      });
      t.get("/instances/:id/elementScreenshot", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { TabManagerServices } = TabManagerModule();
        const image = await TabManagerServices.takeElementScreenshot(
          ctx.params.id,
          sel,
        );
        return { status: 200, body: image != null ? { image } : {} };
      });
      t.get("/instances/:id/fullPageScreenshot", async (ctx: RouterContext) => {
        const { TabManagerServices } = TabManagerModule();
        const image = await TabManagerServices.takeFullPageScreenshot(
          ctx.params.id,
        );
        return { status: 200, body: image != null ? { image } : {} };
      });
      t.post("/instances/:id/regionScreenshot", async (ctx: RouterContext) => {
        const json = ctx.json() as {
          rect?: { x?: number; y?: number; width?: number; height?: number };
        } | null;
        const { TabManagerServices } = TabManagerModule();
        const image = await TabManagerServices.takeRegionScreenshot(
          ctx.params.id,
          json?.rect,
        );
        return { status: 200, body: image != null ? { image } : {} };
      });
      t.post("/instances/:id/fillForm", async (ctx: RouterContext) => {
        const json = ctx.json() as
          | { formData?: { [selector: string]: string } }
          | null;
        const { TabManagerServices } = TabManagerModule();
        const okFilled = await TabManagerServices.fillForm(
          ctx.params.id,
          json?.formData ?? {},
        );
        return { status: 200, body: { ok: okFilled } };
      });
      t.get("/instances/:id/value", async (ctx: RouterContext) => {
        const sel = ctx.searchParams.get("selector") ?? "";
        const { TabManagerServices } = TabManagerModule();
        const value = await TabManagerServices.getValue(ctx.params.id, sel);
        return { status: 200, body: value != null ? { value } : {} };
      });
      t.post("/instances/:id/submit", async (ctx: RouterContext) => {
        const json = ctx.json() as { selector?: string } | null;
        const sel = json?.selector ?? "";
        const { TabManagerServices } = TabManagerModule();
        const submitted = await TabManagerServices.submit(ctx.params.id, sel);
        return { status: 200, body: { ok: submitted } };
      });
    });

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
try {
  // Default: enabled with default port and no token
  const PORT = DEFAULT_PORT;
  const TOKEN = "";
  osLocalServer.start(PORT, TOKEN);
} catch (e) {
  err("failed to start server:", e);
}

// Also export explicit control API
export const startServer = (port = DEFAULT_PORT, token = "") =>
  osLocalServer.start(port, token);
export const stopServer = () => osLocalServer.stop();
