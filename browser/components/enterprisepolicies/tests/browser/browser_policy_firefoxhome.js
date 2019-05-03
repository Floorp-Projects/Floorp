/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_firefox_home_without_policy_without_pocket() {
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: "about:home",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    let search = content.document.querySelector(".search-wrapper");
    isnot(search, null, "Search section should be there.");
    let topsites = content.document.querySelector("section[data-section-id='topsites']");
    isnot(topsites, null, "Top Sites section should be there.");
    let highlights = content.document.querySelector("section[data-section-id='highlights']");
    isnot(highlights, null, "Highlights section should be there.");
  });
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_firefox_home_with_policy() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FirefoxHome": {
        "Search": false,
        "TopSites": false,
        "Highlights": false,
        "Snippets": false,
      },
    },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: "about:home",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    let search = content.document.querySelector(".search-wrapper");
    is(search, null, "Search section should not be there.");
    let topsites = content.document.querySelector("section[data-section-id='topsites']");
    is(topsites, null, "Top Sites section should not be there.");
    let highlights = content.document.querySelector("section[data-section-id='highlights']");
    is(highlights, null, "Highlights section should not be there.");
  });
  BrowserTestUtils.removeTab(tab);
});
