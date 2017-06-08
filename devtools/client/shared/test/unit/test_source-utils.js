/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests utility functions contained in `source-utils.js`
 */

const { require } = Components.utils.import("resource://devtools/shared/Loader.jsm", {});
const sourceUtils = require("devtools/client/shared/source-utils");

function run_test() {
  run_next_test();
}

const CHROME_URLS = [
  "chrome://foo", "resource://baz", "jar:file:///Users/root"
];

const CONTENT_URLS = [
  "http://mozilla.org", "https://mozilla.org", "file:///Users/root", "app://fxosapp",
  "blob:http://mozilla.org", "blob:https://mozilla.org"
];

// Test `sourceUtils.parseURL`
add_task(function* () {
  let parsed = sourceUtils.parseURL("https://foo.com:8888/boo/bar.js?q=query");
  equal(parsed.fileName, "bar.js", "parseURL parsed valid fileName");
  equal(parsed.host, "foo.com:8888", "parseURL parsed valid host");
  equal(parsed.hostname, "foo.com", "parseURL parsed valid hostname");
  equal(parsed.port, "8888", "parseURL parsed valid port");
  equal(parsed.href, "https://foo.com:8888/boo/bar.js?q=query", "parseURL parsed valid href");

  parsed = sourceUtils.parseURL("https://foo.com");
  equal(parsed.host, "foo.com", "parseURL parsed valid host when no port given");
  equal(parsed.hostname, "foo.com", "parseURL parsed valid hostname when no port given");

  equal(sourceUtils.parseURL("self-hosted"), null,
        "parseURL returns `null` for invalid URLs");
});

// Test `sourceUtils.isContentScheme`.
add_task(function* () {
  for (let url of CHROME_URLS) {
    ok(!sourceUtils.isContentScheme(url),
       `${url} correctly identified as not content scheme`);
  }
  for (let url of CONTENT_URLS) {
    ok(sourceUtils.isContentScheme(url), `${url} correctly identified as content scheme`);
  }
});

// Test `sourceUtils.isChromeScheme`.
add_task(function* () {
  for (let url of CHROME_URLS) {
    ok(sourceUtils.isChromeScheme(url), `${url} correctly identified as chrome scheme`);
  }
  for (let url of CONTENT_URLS) {
    ok(!sourceUtils.isChromeScheme(url),
       `${url} correctly identified as not chrome scheme`);
  }
});

// Test `sourceUtils.isWASM`.
add_task(function* () {
  ok(sourceUtils.isWASM("wasm-function[66240] (?:13870536)"),
                        "wasm function correctly identified");
  ok(!sourceUtils.isWASM(CHROME_URLS[0]), `A chrome url does not identify as wasm.`);
});

// Test `sourceUtils.isDataScheme`.
add_task(function* () {
  let dataURI = "data:text/html;charset=utf-8,<!DOCTYPE html></html>";
  ok(sourceUtils.isDataScheme(dataURI), `${dataURI} correctly identified as data scheme`);

  for (let url of CHROME_URLS) {
    ok(!sourceUtils.isDataScheme(url), `${url} correctly identified as not data scheme`);
  }
  for (let url of CONTENT_URLS) {
    ok(!sourceUtils.isDataScheme(url), `${url} correctly identified as not data scheme`);
  }
});

// Test `sourceUtils.getSourceNames`.
add_task(function* () {
  testAbbreviation("http://example.com/foo/bar/baz/boo.js",
                   "boo.js",
                   "http://example.com/foo/bar/baz/boo.js",
                   "example.com");
});

// Test `sourceUtils.isScratchpadTheme`
add_task(function* () {
  ok(sourceUtils.isScratchpadScheme("Scratchpad/1"),
     "Scratchpad/1 identified as scratchpad");
  ok(sourceUtils.isScratchpadScheme("Scratchpad/20"),
     "Scratchpad/20 identified as scratchpad");
  ok(!sourceUtils.isScratchpadScheme("http://www.mozilla.org"), "http://www.mozilla.org not identified as scratchpad");
});

// Test `sourceUtils.getSourceNames`.
add_task(function* () {
  // Check length
  let longMalformedURL = `example.com${new Array(100).fill("/a").join("")}/file.js`;
  ok(sourceUtils.getSourceNames(longMalformedURL).short.length <= 100,
    "`short` names are capped at 100 characters");

  testAbbreviation("self-hosted", "self-hosted", "self-hosted");
  testAbbreviation("", "(unknown)", "(unknown)");

  // Test shortening data URIs, stripping mime/charset
  testAbbreviation("data:text/html;charset=utf-8,<!DOCTYPE html></html>",
                   "data:<!DOCTYPE html></html>",
                   "data:text/html;charset=utf-8,<!DOCTYPE html></html>");

  let longDataURI = `data:image/png;base64,${new Array(100).fill("a").join("")}`;
  let longDataURIShort = sourceUtils.getSourceNames(longDataURI).short;

  // Test shortening data URIs and that the `short` result is capped
  ok(longDataURIShort.length <= 100,
    "`short` names are capped at 100 characters for data URIs");
  equal(longDataURIShort.substr(0, 10), "data:aaaaa",
    "truncated data URI short names still have `data:...`");

  // Test simple URL and cache retrieval by calling the same input multiple times.
  let testUrl = "http://example.com/foo/bar/baz/boo.js";
  testAbbreviation(testUrl, "boo.js", testUrl, "example.com");
  testAbbreviation(testUrl, "boo.js", testUrl, "example.com");

  // Check query and hash and port
  testAbbreviation("http://example.com:8888/foo/bar/baz.js?q=query#go",
                   "baz.js",
                   "http://example.com:8888/foo/bar/baz.js",
                   "example.com:8888");

  // Trailing "/" with nothing beyond host
  testAbbreviation("http://example.com/",
                   "/",
                   "http://example.com/",
                   "example.com");

  // Trailing "/"
  testAbbreviation("http://example.com/foo/bar/",
                   "bar",
                   "http://example.com/foo/bar/",
                   "example.com");

  // Non-extension ending
  testAbbreviation("http://example.com/bar",
                   "bar",
                   "http://example.com/bar",
                   "example.com");

  // Check query
  testAbbreviation("http://example.com/foo.js?bar=1&baz=2",
                   "foo.js",
                   "http://example.com/foo.js",
                   "example.com");

  // Check query with trailing slash
  testAbbreviation("http://example.com/foo/?bar=1&baz=2",
                   "foo",
                   "http://example.com/foo/",
                   "example.com");
});

// Test for source mapped file name
add_task(function* () {
  const { getSourceMappedFile } = sourceUtils;
  const source = "baz.js";
  const output = getSourceMappedFile(source);
  equal(output, "baz.js", "correctly formats file name");
  // Test for OSX file path
  const source1 = "/foo/bar/baz.js";
  const output1 = getSourceMappedFile(source1);
  equal(output1, "baz.js", "correctly formats Linux file path");
  // Test for Windows file path
  const source2 = "Z:\\foo\\bar\\baz.js";
  const output2 = getSourceMappedFile(source2);
  equal(output2, "baz.js", "correctly formats Windows file path");
});

function testAbbreviation(source, short, long, host) {
  let results = sourceUtils.getSourceNames(source);
  equal(results.short, short, `${source} has correct "short" name`);
  equal(results.long, long, `${source} has correct "long" name`);
  equal(results.host, host, `${source} has correct "host" name`);
}
