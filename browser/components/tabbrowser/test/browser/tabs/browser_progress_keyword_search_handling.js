/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

SearchTestUtils.init(this);

const kButton = document.getElementById("reload-button");
const IS_UPGRADING_SCHEMELESS = SpecialPowers.getBoolPref(
  "dom.security.https_first_schemeless"
);
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const DEFAULT_URL_SCHEME = IS_UPGRADING_SCHEMELESS ? "https://" : "http://";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.fixup.dns_first_for_single_words", true]],
  });

  // Create an engine to use for the test.
  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://example.com/",
      search_url_get_params: "q={searchTerms}",
    },
    { setAsDefault: true }
  );
});

/*
 * When loading a keyword search as a result of an unknown host error,
 * check that we can stop the load.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=235825
 */
add_task(async function test_unknown_host() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    const kNonExistingHost = "idontreallyexistonthisnetwork";
    let searchPromise = BrowserTestUtils.browserStarted(
      browser,
      Services.uriFixup.keywordToURI(kNonExistingHost).preferredURI.spec
    );

    gURLBar.value = kNonExistingHost;
    gURLBar.select();
    EventUtils.synthesizeKey("KEY_Enter");

    await searchPromise;
    // With parent initiated loads, we need to give XULBrowserWindow
    // time to process the STATE_START event and set the attribute to true.
    await new Promise(resolve => executeSoon(resolve));

    ok(kButton.hasAttribute("displaystop"), "Should be showing stop");

    await TestUtils.waitForCondition(
      () => !kButton.hasAttribute("displaystop")
    );
    ok(
      !kButton.hasAttribute("displaystop"),
      "Should no longer be showing stop after search"
    );
  });
});

/*
 * When NOT loading a keyword search as a result of an unknown host error,
 * check that the stop button goes back to being a reload button.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1591183
 */
add_task(async function test_unknown_host_without_search() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    const kNonExistingHost = "idontreallyexistonthisnetwork.example.com";
    let searchPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      DEFAULT_URL_SCHEME + kNonExistingHost + "/",
      true /* want an error page */
    );
    gURLBar.value = kNonExistingHost;
    gURLBar.select();
    EventUtils.synthesizeKey("KEY_Enter");
    await searchPromise;
    await TestUtils.waitForCondition(
      () => !kButton.hasAttribute("displaystop")
    );
    ok(
      !kButton.hasAttribute("displaystop"),
      "Should not be showing stop on error page"
    );
  });
});
