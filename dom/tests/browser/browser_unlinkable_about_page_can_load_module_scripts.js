/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const kTestPage = getRootDirectory(gTestPath) + "file_load_module_script.html";

const kDefaultFlags =
  Ci.nsIAboutModule.ALLOW_SCRIPT |
  Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
  Ci.nsIAboutModule.URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
  Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT;

const kAboutPagesRegistered = Promise.all([
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction,
    "test-linkable-page",
    kTestPage,
    kDefaultFlags | Ci.nsIAboutModule.MAKE_LINKABLE
  ),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction,
    "test-unlinkable-page",
    kTestPage,
    kDefaultFlags
  ),
]);

add_task(async function () {
  await kAboutPagesRegistered;

  let consoleListener = {
    observe() {
      errorCount++;
      if (shouldHaveJSError) {
        ok(true, "JS error is expected and received");
      } else {
        ok(false, "Unexpected JS error was received");
      }
    },
  };
  Services.console.registerListener(consoleListener);
  registerCleanupFunction(() =>
    Services.console.unregisterListener(consoleListener)
  );

  let shouldHaveJSError = true;
  let errorCount = 0;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:test-linkable-page" },
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        isnot(
          content.document.body.textContent,
          "scriptLoaded",
          "The page content shouldn't be changed on the linkable page"
        );
      });
    }
  );
  ok(
    errorCount > 0,
    "Should have an error when loading test-linkable-page, got " + errorCount
  );

  shouldHaveJSError = false;
  errorCount = 0;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:test-unlinkable-page" },
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        is(
          content.document.body.textContent,
          "scriptLoaded",
          "The page content should be changed on the unlinkable page"
        );
      });
    }
  );
  is(errorCount, 0, "Should have no errors when loading test-unlinkable-page");
});
