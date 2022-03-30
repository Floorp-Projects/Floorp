/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Nested links should only open a single tab when ctrl-clicked.
 */
add_task(async function nested_link_click_opens_single_tab() {
  await BrowserTestUtils.withNewTab(
    "https://example.com/empty",
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        let doc = content.document;
        let outerLink = doc.createElement("a");
        outerLink.href = "https://mozilla.org/";
        let link = doc.createElement("a");
        link.href = "https://example.org/linked";
        link.textContent = "Click me";
        link.id = "mylink";
        outerLink.append(link);
        doc.body.append(outerLink);
      });

      let startingNumberOfTabs = gBrowser.tabs.length;
      let newTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        "https://example.org/linked",
        true
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#mylink",
        { accelKey: true },
        browser
      );
      let tab = await newTabPromise;
      is(
        gBrowser.tabs.length,
        startingNumberOfTabs + 1,
        "Should only have opened 1 tab."
      );
      BrowserTestUtils.removeTab(tab);
    }
  );
});
