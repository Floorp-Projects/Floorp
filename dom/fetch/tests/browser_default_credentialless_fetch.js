/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RESOURCE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "store_header.sjs";

add_task(async function test_fetch_defaults_to_credentialless() {
  // Ensure cookie is set up:
  let expiry = Date.now() / 1000 + 24 * 60 * 60;
  Services.cookies.add(
    "example.com",
    "/",
    "foo",
    "bar",
    false,
    false,
    false,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );

  // Explicitly send cookie header by using `same-origin` in the init dict, to
  // ensure cookies are stored correctly and can be sent.
  await fetch(RESOURCE_URL + "?checkheader", { credentials: "same-origin" });

  Assert.equal(
    await fetch(RESOURCE_URL + "?getstate").then(r => r.text()),
    "hasCookie",
    "Should have cookie when explicitly passing credentials info in 'checkheader' request."
  );

  // Check the default behaviour.
  await fetch(RESOURCE_URL + "?checkheader");
  Assert.equal(
    await fetch(RESOURCE_URL + "?getstate").then(r => r.text()),
    "noCookie",
    "Should not have cookie in the default case (no explicit credentials mode) for chrome privileged requests."
  );
});
