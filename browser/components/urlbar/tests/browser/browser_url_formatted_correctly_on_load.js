/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  XPCShellContentUtils:
    "resource://testing-common/XPCShellContentUtils.sys.mjs",
});

let PUNYCODE_PAGE = "xn--31b1c3b9b.com";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
let DECODED_PAGE = "http://योगा.com/";

function startServer() {
  XPCShellContentUtils.ensureInitialized(this);
  let server = XPCShellContentUtils.createHttpServer({
    hosts: [PUNYCODE_PAGE],
  });
  server.registerPathHandler("/", (request, response) => {
    response.write("<html>A page without icon</html>");
  });
}

add_task(async function test_url_formatted_correctly_on_page_load() {
  SpecialPowers.pushPrefEnv({ set: [["browser.urlbar.trimURLs", false]] });
  startServer();

  let onValueChangeCalledAtLeastOnce = false;
  let onValueChanged = _ => {
    is(gURLBar.value, DECODED_PAGE, "Value is decoded.");
    onValueChangeCalledAtLeastOnce = true;
  };

  gURLBar.inputField.addEventListener("ValueChange", onValueChanged);
  registerCleanupFunction(() => {
    gURLBar.inputField.removeEventListener("ValueChange", onValueChanged);
  });

  BrowserTestUtils.startLoadingURIString(gBrowser, PUNYCODE_PAGE);
  // Check that whenever the value of the urlbar is changed, the correct
  // decoded punycode url is used.
  await BrowserTestUtils.browserLoaded(gBrowser, false, null, true);

  ok(
    onValueChangeCalledAtLeastOnce,
    "OnValueChanged of UrlbarInput was called at least once."
  );
  // Check that the final value is decoded punycode as well.
  is(gURLBar.value, DECODED_PAGE, "Final Urlbar value is correct");

  // Cleanup.
  SpecialPowers.popPrefEnv();
});
