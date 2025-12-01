/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * HTTP utility functions for the Floorp OS Local HTTP Server
 */

import type { HeadersMap, HttpRequest } from "./types.ts";

// -- Logging utilities --------------------------------------------------------

export function log(...args: unknown[]) {
  console.log("[Floorp OS-Server]", ...args);
}

export function warn(...args: unknown[]) {
  console.warn("[Floorp OS-Server]", ...args);
}

export function err(...args: unknown[]) {
  console.error("[Floorp OS-Server]", ...args);
}

// -- HTTP parsing -------------------------------------------------------------

export function parseHeaders(raw: string): HeadersMap {
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

export function parseRequestHead(head: string): HttpRequest | null {
  const [reqLine, ...headerLines] = head.split("\r\n");
  const parts = reqLine.split(" ");
  if (parts.length < 3) return null;
  const method = parts[0];
  const path = parts[1] || "/";
  const headers = parseHeaders(headerLines.join("\r\n"));
  return { method, path, headers, body: new Uint8Array() };
}

export function getBodyText(req: HttpRequest): string {
  try {
    const dec = new TextDecoder();
    return dec.decode(req.body);
  } catch (e) {
    void e;
    return "";
  }
}

// Convert a binary string (from nsIScriptableInputStream.read) to bytes
export function binaryStringToByteArray(s: string): number[] {
  const out = Array.from({ length: s.length }, () => 0) as number[];
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
  return out;
}

export function getJSON<T = unknown>(req: HttpRequest): T | null {
  const ct = (req.headers["content-type"] || "").toLowerCase();
  if (!ct.includes("application/json")) return null;
  const txt = getBodyText(req).trim();
  if (!txt) return null;
  try {
    return JSON.parse(txt) as T;
  } catch (e) {
    void e;
    return null;
  }
}

// -- HTTP response builders ---------------------------------------------------

export function statusText(code: number): string {
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

export function jsonResponse(
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
  const head =
    `HTTP/1.1 ${status} ${statusText(status)}\r\n` +
    Object.entries(headers)
      .map(([k, v]) => `${k}: ${v}`)
      .join("\r\n") +
    "\r\n\r\n";
  return head + body;
}

// Write UTF-8 bytes to an nsIOutputStream safely
export function writeUtf8(out: nsIOutputStream, text: string) {
  const enc = new TextEncoder();
  const bytes = enc.encode(text);
  const bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream,
  );
  bos.setOutputStream(out);
  // nsIBinaryOutputStream#writeByteArray expects a u8[]; convert from Uint8Array
  bos.writeByteArray(Array.from(bytes));
}

// -- Standard response helpers ------------------------------------------------

export function badRequest(msg = "bad request"): string {
  return jsonResponse(400, { error: msg });
}

export function requestTimeout(msg = "request timeout"): string {
  return jsonResponse(408, { error: msg });
}

export function unauthorized(msg = "unauthorized"): string {
  return jsonResponse(401, { error: msg });
}

export function notFound(): string {
  return jsonResponse(404, { error: "not found" });
}

export function payloadTooLarge(msg = "payload too large"): string {
  return jsonResponse(413, { error: msg });
}

export function serverError(msg = "internal error"): string {
  return jsonResponse(500, { error: msg });
}

export function corsPreflight(): string {
  const headers: HeadersMap = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, DELETE, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
    "Content-Length": "0",
    Connection: "close",
  };
  const head =
    `HTTP/1.1 204 No Content\r\n` +
    Object.entries(headers)
      .map(([k, v]) => `${k}: ${v}`)
      .join("\r\n") +
    "\r\n\r\n";
  return head;
}

// -- URL parsing --------------------------------------------------------------

export function parseURL(path: string): {
  pathname: string;
  searchParams: URLSearchParams;
} {
  // Use a dummy origin to parse
  const u = new URL(`http://localhost${path}`);
  return { pathname: u.pathname, searchParams: u.searchParams };
}

// -- Number clamping ----------------------------------------------------------

export function clampInt(
  valueStr: string | null,
  def: number,
  min: number,
  max: number,
): number {
  const n = Number(valueStr ?? def);
  if (!Number.isFinite(n)) return def;
  return Math.max(min, Math.min(max, Math.floor(n)));
}
