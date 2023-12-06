/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtabpage.activity-stream.feeds.section.highlights", true],
    ],
  });
});

add_task(async function test_firefox_home_without_policy_without_pocket() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:home",
    waitForStateStop: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    let search = content.document.querySelector(".search-wrapper");
    isnot(search, null, "Search section should be there.");
    let topsites = content.document.querySelector(
      "section[data-section-id='topsites']"
    );
    isnot(topsites, null, "Top Sites section should be there.");
    let highlights = content.document.querySelector(
      "section[data-section-id='highlights']"
    );
    isnot(highlights, null, "Highlights section should be there.");
  });
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_firefox_home_with_policy() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.discoverystream.endpointSpocsClear",
        "",
      ],
    ],
  });

  await setupPolicyEngineWithJson({
    policies: {
      FirefoxHome: {
        Search: false,
        TopSites: false,
        Highlights: false,
      },
    },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:home",
    waitForStateStop: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    let search = content.document.querySelector(".search-wrapper");
    is(search, null, "Search section should not be there.");
    let topsites = content.document.querySelector(
      "section[data-section-id='topsites']"
    );
    is(topsites, null, "Top Sites section should not be there.");
    let highlights = content.document.querySelector(
      "section[data-section-id='highlights']"
    );
    is(highlights, null, "Highlights section should not be there.");
  });
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_firefoxhome_preferences_set() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.discoverystream.endpointSpocsClear",
        "",
      ],
    ],
  });

  await setupPolicyEngineWithJson({
    policies: {
      FirefoxHome: {
        Search: false,
        TopSites: false,
        SponsoredTopSites: false,
        Highlights: false,
        Pocket: false,
        SponsoredPocket: false,
        Locked: true,
      },
    },
  });

  await BrowserTestUtils.withNewTab("about:preferences#home", async browser => {
    let data = {
      Search: "browser.newtabpage.activity-stream.showSearch",
      TopSites: "browser.newtabpage.activity-stream.feeds.topsites",
      SponsoredTopSites:
        "browser.newtabpage.activity-stream.showSponsoredTopSites",
      Highlights: "browser.newtabpage.activity-stream.feeds.section.highlights",
      Pocket: "browser.newtabpage.activity-stream.feeds.section.topstories",
      SponsoredPocket: "browser.newtabpage.activity-stream.showSponsored",
    };
    for (let [section, preference] of Object.entries(data)) {
      is(
        browser.contentDocument.querySelector(
          `checkbox[preference='${preference}']`
        ).disabled,
        true,
        `${section} checkbox should be disabled`
      );
    }
  });
  await setupPolicyEngineWithJson({
    policies: {
      FirefoxHome: {},
    },
  });
  await SpecialPowers.popPrefEnv();
});
