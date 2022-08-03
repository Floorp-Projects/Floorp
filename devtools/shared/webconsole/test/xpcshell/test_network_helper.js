/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Object.defineProperty(this, "NetworkHelper", {
  get() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true,
});

function run_test() {
  test_isTextMimeType();
  test_parseCookieHeader();
}

function test_isTextMimeType() {
  Assert.equal(NetworkHelper.isTextMimeType("text/plain"), true);
  Assert.equal(NetworkHelper.isTextMimeType("application/javascript"), true);
  Assert.equal(NetworkHelper.isTextMimeType("application/json"), true);
  Assert.equal(NetworkHelper.isTextMimeType("text/css"), true);
  Assert.equal(NetworkHelper.isTextMimeType("text/html"), true);
  Assert.equal(NetworkHelper.isTextMimeType("image/svg+xml"), true);
  Assert.equal(NetworkHelper.isTextMimeType("application/xml"), true);

  // Test custom JSON subtype
  Assert.equal(
    NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0+json"),
    true
  );
  Assert.equal(
    NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0-json"),
    true
  );
  // Test custom XML subtype
  Assert.equal(
    NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0+xml"),
    true
  );
  Assert.equal(
    NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0-xml"),
    false
  );
  // Test case-insensitive
  Assert.equal(
    NetworkHelper.isTextMimeType("application/vnd.BIG-CORP+json"),
    true
  );
  // Test non-text type
  Assert.equal(NetworkHelper.isTextMimeType("image/png"), false);
  // Test invalid types
  Assert.equal(NetworkHelper.isTextMimeType("application/foo-+json"), false);
  Assert.equal(NetworkHelper.isTextMimeType("application/-foo+json"), false);
  Assert.equal(
    NetworkHelper.isTextMimeType("application/foo--bar+json"),
    false
  );

  // Test we do not cause internal errors with unoptimized regex. Bug 961097
  Assert.equal(
    NetworkHelper.isTextMimeType("application/vnd.google.safebrowsing-chunk"),
    false
  );
}

function test_parseCookieHeader() {
  let result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=Strict");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "Strict" }]);

  result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=strict");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "Strict" }]);

  result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=STRICT");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "Strict" }]);

  result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=None");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "None" }]);

  result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=NONE");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "None" }]);

  result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=lax");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "Lax" }]);

  result = NetworkHelper.parseSetCookieHeader("Test=1; SameSite=Lax");
  Assert.deepEqual(result, [{ name: "Test", value: "1", samesite: "Lax" }]);
}
