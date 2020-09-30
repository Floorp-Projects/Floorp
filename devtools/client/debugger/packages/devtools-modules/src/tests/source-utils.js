/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  getSourceMappedFile,
  getSourceNames,
  isChromeScheme,
  isContentScheme,
  isDataScheme,
  isWASM,
  parseURL,
} = require("../source-utils");

describe("source-utils", () => {
  const CHROME_URLS = [
    "chrome://foo",
    "resource://baz",
    "jar:file:///Users/root"
  ];
  const CONTENT_URLS = [
    "http://mozilla.org",
    "https://mozilla.org",
    "file:///Users/root",
    "app://fxosapp",
    "blob:http://mozilla.org",
    "blob:https://mozilla.org"
  ];

  it("parse URLs", () => {
    let parsed = parseURL("https://foo.com:8888/boo/bar.js?q=query");

    expect(parsed.fileName).toBe("bar.js");
    expect(parsed.host).toBe("foo.com:8888");
    expect(parsed.hostname).toBe("foo.com");
    expect(parsed.port).toBe("8888");
    expect(parsed.href).toBe("https://foo.com:8888/boo/bar.js?q=query");

    parsed = parseURL("https://foo.com");
    expect(parsed.host).toBe("foo.com");
    expect(parsed.hostname).toBe("foo.com");

    expect(parseURL("self-hosted")).toBe(null);
  });

  it("isContentScheme", () => {
    for (let url of CHROME_URLS) {
      expect(isContentScheme(url)).toBe(false);
    }
    for (let url of CONTENT_URLS) {
      expect(isContentScheme(url)).toBe(true);
    }
  });

  it("isChromeScheme", () => {
    for (let url of CHROME_URLS) {
      expect(isChromeScheme(url)).toBe(true);
    }
    for (let url of CONTENT_URLS) {
      expect(isChromeScheme(url)).toBe(false);
    }
  });

  it("isWASM", () => {
    expect(isWASM("wasm-function[66240] (?:13870536)")).toBe(true);
    expect(isWASM(CHROME_URLS[0])).toBe(false);
  });

  it("isDataScheme", () => {
    const dataURI = "data:text/html;charset=utf-8,<!DOCTYPE html></html>";
    expect(isDataScheme(dataURI)).toBe(true);

    for (let url of CHROME_URLS) {
      expect(isDataScheme(url)).toBe(false);
    }
    for (let url of CONTENT_URLS) {
      expect(isDataScheme(url)).toBe(false);
    }
  });

  it("getSourceMappedFile", () => {
    expect(getSourceMappedFile("baz.js")).toBe("baz.js");
    expect(getSourceMappedFile("/foo/bar/baz.js")).toBe("baz.js");
    expect(getSourceMappedFile("Z:\\foo\\bar\\baz.js")).toBe("baz.js");
  });

  it("getSourceNames", () => {
    // Check length
    const longMalformedURL = `example.com${"/a".repeat(100)}/file.js`;
    expect(getSourceNames(longMalformedURL).short.length).toBeLessThanOrEqual(100);

    expect(getSourceNames("http://example.com/foo/bar/baz/boo.js")).toEqual({
      short: "boo.js",
      long: "http://example.com/foo/bar/baz/boo.js",
      host: "example.com"
    });

    expect(getSourceNames("self-hosted")).toEqual({
      short: "self-hosted",
      long: "self-hosted",
      host: undefined
    });

    expect(getSourceNames("")).toEqual({
      short: "(unknown)",
      long: "(unknown)",
      host: undefined
    });

    // Test shortening data URIs, stripping mime/charset
    const dataURI = "data:text/html;charset=utf-8,<!DOCTYPE html></html>";
    expect(getSourceNames(dataURI)).toEqual({
      short: "data:<!DOCTYPE html></html>",
      long: "data:text/html;charset=utf-8,<!DOCTYPE html></html>",
      host: undefined
    });

    // Test shortening data URIs and that the `short` result is capped
    let longDataURI = `data:image/png;base64,${"a".repeat(100)}`;
    let longDataURIShort = getSourceNames(longDataURI).short;
    expect(longDataURIShort.length).toBeLessThanOrEqual(100);
    expect(longDataURIShort.substr(0, 10)).toBe("data:aaaaa");

    // Test simple URL and cache retrieval by calling the same input multiple times.
    let testUrl = "http://example.com/foo/bar/baz/boo.js";
    expect(getSourceNames(testUrl)).toEqual({
      short: "boo.js",
      long: testUrl,
      host: "example.com"
    });
    expect(getSourceNames(testUrl)).toEqual({
      short: "boo.js",
      long: testUrl,
      host: "example.com"
    });

    // Check query and hash and port
    expect(getSourceNames("http://example.com:8888/foo/bar/baz.js?q=query#go")).toEqual({
      short: "baz.js",
      long: "http://example.com:8888/foo/bar/baz.js",
      host: "example.com:8888"
    });

    // Trailing "/" with nothing beyond host
    expect(getSourceNames("http://example.com/")).toEqual({
      short: "/",
      long: "http://example.com/",
      host: "example.com"
    });

    // Trailing "/"
    expect(getSourceNames("http://example.com/foo/bar/")).toEqual({
      short: "bar",
      long: "http://example.com/foo/bar/",
      host: "example.com"
    });

    // Non-extension ending
    expect(getSourceNames("http://example.com/bar")).toEqual({
      short: "bar",
      long: "http://example.com/bar",
      host: "example.com"
    });

    // Check query
    expect(getSourceNames("http://example.com/foo.js?bar=1&baz=2")).toEqual({
      short: "foo.js",
      long: "http://example.com/foo.js",
      host: "example.com"
    });

    // Check query with trailing slash
    expect(getSourceNames("http://example.com/foo/?bar=1&baz=2")).toEqual({
      short: "foo",
      long: "http://example.com/foo/",
      host: "example.com"
    });
  });
});
