/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests utility functions contained in `source-utils.js`
 */

const Cu = Components.utils;
const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const sourceUtils = require("devtools/client/shared/source-utils");

function run_test() {
  run_next_test();
}

const CHROME_URLS = [
  "chrome://foo", "resource://baz", "jar:file:///Users/root"
];

const CONTENT_URLS = [
  "http://mozilla.org", "https://mozilla.org", "file:///Users/root", "app://fxosapp"
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

  equal(sourceUtils.parseURL("self-hosted"), null, "parseURL returns `null` for invalid URLs");
});

// Test `sourceUtils.isContentScheme`.
add_task(function* () {
  for (let url of CHROME_URLS) {
    ok(!sourceUtils.isContentScheme(url), `${url} correctly identified as not content scheme`);
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
    ok(!sourceUtils.isChromeScheme(url), `${url} correctly identified as not chrome scheme`);
  }
});

// Test `sourceUtils.getSourceNames`.
add_task(function* () {
  const url = "http://example.com:8888/foo/bar/baz.js";
  let results = sourceUtils.getSourceNames(url);
  equal(results.short, "baz.js");
  equal(results.long, url);
  equal(results.host, "example.com:8888");

  results = sourceUtils.getSourceNames("self-hosted");
  equal(results.short, "self-hosted");
  equal(results.long, "self-hosted");
  equal(results.host, undefined);

  results = sourceUtils.getSourceNames("", "<unknown>");
  equal(results.short, "<unknown>");
  equal(results.long, "<unknown>");
  equal(results.host, undefined);
});
