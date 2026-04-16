// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  parseHeaders,
  parseRequestHead,
  getBodyText,
  binaryStringToByteArray,
  getJSON,
  statusText,
  jsonResponse,
  badRequest,
  notFound,
  parseURL,
  clampInt,
} from "../../http/utils.sys.mts";
import type { HttpRequest } from "../../http/types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../../chrome/test/utils/test_harness.ts";

function makeRequest(overrides: Partial<HttpRequest> = {}): HttpRequest {
  return {
    method: "GET",
    path: "/",
    headers: {},
    body: new Uint8Array(),
    ...overrides,
  };
}

// ---------------------------------------------------------------------------
// Tests — parseHeaders
// ---------------------------------------------------------------------------

function testParseHeadersSingle(): void {
  const headers = parseHeaders("Content-Type: application/json");
  assertEquals(
    headers["content-type"],
    "application/json",
    "should parse single header (lowercase key)",
  );
}

function testParseHeadersMultiple(): void {
  const raw = "Content-Type: text/html\r\nX-Custom: value";
  const headers = parseHeaders(raw);
  assertEquals(headers["content-type"], "text/html", "first header");
  assertEquals(headers["x-custom"], "value", "second header");
}

function testParseHeadersEmpty(): void {
  const headers = parseHeaders("");
  assertEquals(
    Object.keys(headers).length,
    0,
    "empty string should produce empty object",
  );
}

function testParseHeadersTrimsWhitespace(): void {
  const headers = parseHeaders("  Host :  example.com  ");
  assertEquals(headers["host"], "example.com", "should trim whitespace");
}

// ---------------------------------------------------------------------------
// Tests — parseRequestHead
// ---------------------------------------------------------------------------

function testParseRequestHeadValid(): void {
  const head = "GET /api/tabs HTTP/1.1\r\nHost: localhost";
  const req = parseRequestHead(head);
  assert(req !== null, "should parse valid request");
  assertEquals(req.method, "GET", "method");
  assertEquals(req.path, "/api/tabs", "path");
  assertEquals(req.headers["host"], "localhost", "host header");
}

function testParseRequestHeadPost(): void {
  const head = "POST /api/data HTTP/1.1\r\nContent-Type: application/json";
  const req = parseRequestHead(head);
  assert(req !== null, "should parse POST");
  assertEquals(req.method, "POST", "method should be POST");
}

function testParseRequestHeadInvalid(): void {
  const req = parseRequestHead("BROKEN");
  assertEquals(req, null, "invalid request line should return null");
}

// ---------------------------------------------------------------------------
// Tests — getBodyText
// ---------------------------------------------------------------------------

function testGetBodyTextUtf8(): void {
  const body = new TextEncoder().encode("hello world");
  const req = makeRequest({ body });
  assertEquals(getBodyText(req), "hello world", "should decode UTF-8 body");
}

function testGetBodyTextEmpty(): void {
  const req = makeRequest();
  assertEquals(getBodyText(req), "", "empty body should return empty string");
}

// ---------------------------------------------------------------------------
// Tests — binaryStringToByteArray
// ---------------------------------------------------------------------------

function testBinaryStringToByteArray(): void {
  const result = binaryStringToByteArray("AB");
  assertEquals(result.length, 2, "should have 2 bytes");
  assertEquals(result[0], 65, "A = 65");
  assertEquals(result[1], 66, "B = 66");
}

function testBinaryStringToByteArrayEmpty(): void {
  const result = binaryStringToByteArray("");
  assertEquals(result.length, 0, "empty string should return empty array");
}

// ---------------------------------------------------------------------------
// Tests — getJSON
// ---------------------------------------------------------------------------

function testGetJSONValid(): void {
  const body = new TextEncoder().encode('{"key":"value"}');
  const req = makeRequest({
    headers: { "content-type": "application/json" },
    body,
  });
  const result = getJSON<{ key: string }>(req);
  assert(result !== null, "should parse JSON");
  assertEquals(result.key, "value", "should have correct value");
}

function testGetJSONWrongContentType(): void {
  const body = new TextEncoder().encode('{"key":"value"}');
  const req = makeRequest({
    headers: { "content-type": "text/plain" },
    body,
  });
  assertEquals(getJSON(req), null, "wrong content-type should return null");
}

function testGetJSONInvalidBody(): void {
  const body = new TextEncoder().encode("not-json{");
  const req = makeRequest({
    headers: { "content-type": "application/json" },
    body,
  });
  assertEquals(getJSON(req), null, "invalid JSON should return null");
}

function testGetJSONEmptyBody(): void {
  const req = makeRequest({
    headers: { "content-type": "application/json" },
  });
  assertEquals(getJSON(req), null, "empty body should return null");
}

// ---------------------------------------------------------------------------
// Tests — statusText
// ---------------------------------------------------------------------------

function testStatusText200(): void {
  assertEquals(statusText(200), "OK", "200 → OK");
}

function testStatusText404(): void {
  assertEquals(statusText(404), "Not Found", "404 → Not Found");
}

function testStatusText500(): void {
  assertEquals(statusText(500), "Internal Server Error", "500 → ISE");
}

function testStatusTextUnknown(): void {
  assertEquals(statusText(999), "", "unknown code → empty string");
}

// ---------------------------------------------------------------------------
// Tests — jsonResponse
// ---------------------------------------------------------------------------

function testJsonResponseContainsStatusLine(): void {
  const resp = jsonResponse(200, { ok: true });
  assert(resp.startsWith("HTTP/1.1 200 OK"), "should start with status line");
}

function testJsonResponseContainsBody(): void {
  const resp = jsonResponse(200, { ok: true });
  assert(resp.includes('{"ok":true}'), "should contain JSON body");
}

function testJsonResponseContentType(): void {
  const resp = jsonResponse(200, {});
  assert(
    resp.includes("Content-Type: application/json"),
    "should have JSON content-type",
  );
}

// ---------------------------------------------------------------------------
// Tests — helper responses
// ---------------------------------------------------------------------------

function testBadRequestDefault(): void {
  const resp = badRequest();
  assert(resp.includes("400"), "should contain 400");
  assert(resp.includes("bad request"), "should contain default message");
}

function testNotFoundResponse(): void {
  const resp = notFound();
  assert(resp.includes("404"), "should contain 404");
}

// ---------------------------------------------------------------------------
// Tests — parseURL
// ---------------------------------------------------------------------------

function testParseURLSimple(): void {
  const { pathname, searchParams } = parseURL("/api/tabs");
  assertEquals(pathname, "/api/tabs", "pathname");
  assertEquals(searchParams.toString(), "", "no params");
}

function testParseURLWithParams(): void {
  const { pathname, searchParams } = parseURL("/api/tabs?limit=10&offset=5");
  assertEquals(pathname, "/api/tabs", "pathname");
  assertEquals(searchParams.get("limit"), "10", "limit param");
  assertEquals(searchParams.get("offset"), "5", "offset param");
}

// ---------------------------------------------------------------------------
// Tests — clampInt
// ---------------------------------------------------------------------------

function testClampIntNormal(): void {
  assertEquals(clampInt("50", 0, 1, 100), 50, "50 in [1,100] should be 50");
}

function testClampIntBelowMin(): void {
  assertEquals(clampInt("-5", 0, 1, 100), 1, "-5 should clamp to 1");
}

function testClampIntAboveMax(): void {
  assertEquals(clampInt("200", 0, 1, 100), 100, "200 should clamp to 100");
}

function testClampIntNull(): void {
  assertEquals(clampInt(null, 42, 1, 100), 42, "null should use default");
}

function testClampIntNaN(): void {
  assertEquals(clampInt("abc", 42, 1, 100), 42, "NaN should use default");
}

function testClampIntFloored(): void {
  assertEquals(clampInt("3.7", 0, 1, 100), 3, "should floor to 3");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "parseHeaders single", fn: testParseHeadersSingle },
    { name: "parseHeaders multiple", fn: testParseHeadersMultiple },
    { name: "parseHeaders empty", fn: testParseHeadersEmpty },
    { name: "parseHeaders trims", fn: testParseHeadersTrimsWhitespace },
    { name: "parseRequestHead valid", fn: testParseRequestHeadValid },
    { name: "parseRequestHead POST", fn: testParseRequestHeadPost },
    { name: "parseRequestHead invalid", fn: testParseRequestHeadInvalid },
    { name: "getBodyText UTF-8", fn: testGetBodyTextUtf8 },
    { name: "getBodyText empty", fn: testGetBodyTextEmpty },
    { name: "binaryStringToByteArray", fn: testBinaryStringToByteArray },
    {
      name: "binaryStringToByteArray empty",
      fn: testBinaryStringToByteArrayEmpty,
    },
    { name: "getJSON valid", fn: testGetJSONValid },
    { name: "getJSON wrong content-type", fn: testGetJSONWrongContentType },
    { name: "getJSON invalid body", fn: testGetJSONInvalidBody },
    { name: "getJSON empty body", fn: testGetJSONEmptyBody },
    { name: "statusText 200", fn: testStatusText200 },
    { name: "statusText 404", fn: testStatusText404 },
    { name: "statusText 500", fn: testStatusText500 },
    { name: "statusText unknown", fn: testStatusTextUnknown },
    {
      name: "jsonResponse status line",
      fn: testJsonResponseContainsStatusLine,
    },
    { name: "jsonResponse body", fn: testJsonResponseContainsBody },
    { name: "jsonResponse content-type", fn: testJsonResponseContentType },
    { name: "badRequest default", fn: testBadRequestDefault },
    { name: "notFound", fn: testNotFoundResponse },
    { name: "parseURL simple", fn: testParseURLSimple },
    { name: "parseURL with params", fn: testParseURLWithParams },
    { name: "clampInt normal", fn: testClampIntNormal },
    { name: "clampInt below min", fn: testClampIntBelowMin },
    { name: "clampInt above max", fn: testClampIntAboveMax },
    { name: "clampInt null", fn: testClampIntNull },
    { name: "clampInt NaN", fn: testClampIntNaN },
    { name: "clampInt floored", fn: testClampIntFloored },
  ];

  await runTests("utils.test.mts", tests);
}
