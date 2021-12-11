/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AboutNewTab } = ChromeUtils.import(
  "resource:///modules/AboutNewTab.jsm"
);
const { PlacesTestUtils } = ChromeUtils.import(
  "resource://testing-common/PlacesTestUtils.jsm"
);
const { PlacesUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesUtils.jsm"
);
const {
  ExtensionUtils: { makeDataURI },
} = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

// A small 1x1 test png
const IMAGE_1x1 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";

async function updateTopSites(condition) {
  // Toggle the pref to clear the feed cache and force an update.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtabpage.activity-stream.feeds.system.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", true],
    ],
  });

  // Wait for the feed to be updated.
  await TestUtils.waitForCondition(() => {
    let sites = AboutNewTab.getTopSites();
    return condition(sites);
  }, "Waiting for top sites to be updated");
}

async function loadExtension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["topSites"],
    },
    background() {
      browser.test.onMessage.addListener(async options => {
        let sites;
        if (typeof options !== undefined) {
          sites = await browser.topSites.get(options);
        } else {
          sites = await browser.topSites.get();
        }
        browser.test.sendMessage("sites", sites);
      });
    },
  });
  await extension.startup();
  return extension;
}

async function getSites(extension, options) {
  extension.sendMessage(options);
  return extension.awaitMessage("sites");
}

add_task(async function setup() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await SpecialPowers.pushPrefEnv({
    set: [
      // The pref for TopSites is empty by default.
      [
        "browser.newtabpage.activity-stream.default.sites",
        "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/",
      ],
      // Toggle the feed off and on as a workaround to read the new prefs.
      ["browser.newtabpage.activity-stream.feeds.system.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", true],
      [
        "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
        true,
      ],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
});

// Tests newtab links with an empty history.
add_task(async function test_topSites_newtab_emptyHistory() {
  let extension = await loadExtension();

  let expectedResults = [
    {
      type: "search",
      url: "https://amazon.com",
      title: "@amazon",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.youtube.com/",
      title: "youtube",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.facebook.com/",
      title: "facebook",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.reddit.com/",
      title: "reddit",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.wikipedia.org/",
      title: "wikipedia",
      favicon: null,
    },
    {
      type: "url",
      url: "https://twitter.com/",
      title: "twitter",
      favicon: null,
    },
  ];

  Assert.deepEqual(
    expectedResults,
    await getSites(extension, { newtab: true }),
    "got topSites newtab links"
  );

  await extension.unload();
});

// Tests newtab links with some visits.
add_task(async function test_topSites_newtab_visits() {
  let extension = await loadExtension();

  // Add some visits to a couple of URLs.  We need to add at least two visits
  // per URL for it to show up.  Add some extra to be safe, and add one more to
  // the first so that its frecency is larger.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      "http://example-1.com/",
      "http://example-2.com/",
    ]);
  }
  await PlacesTestUtils.addVisits("http://example-1.com/");

  // Wait for example-1.com to be listed second, after the Amazon search link.
  await updateTopSites(sites => {
    return sites && sites[1] && sites[1].url == "http://example-1.com/";
  });

  let expectedResults = [
    {
      type: "search",
      url: "https://amazon.com",
      title: "@amazon",
      favicon: null,
    },
    {
      type: "url",
      url: "http://example-1.com/",
      title: "test visit for http://example-1.com/",
      favicon: null,
    },
    {
      type: "url",
      url: "http://example-2.com/",
      title: "test visit for http://example-2.com/",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.youtube.com/",
      title: "youtube",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.facebook.com/",
      title: "facebook",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.reddit.com/",
      title: "reddit",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.wikipedia.org/",
      title: "wikipedia",
      favicon: null,
    },
    {
      type: "url",
      url: "https://twitter.com/",
      title: "twitter",
      favicon: null,
    },
  ];

  Assert.deepEqual(
    expectedResults,
    await getSites(extension, { newtab: true }),
    "got topSites newtab links"
  );

  await extension.unload();
  await PlacesUtils.history.clear();
});

// Tests that the newtab parameter is ignored if newtab Top Sites are disabled.
add_task(async function test_topSites_newtab_ignored() {
  let extension = await loadExtension();
  // Add some visits to a couple of URLs.  We need to add at least two visits
  // per URL for it to show up.  Add some extra to be safe, and add one more to
  // the first so that its frecency is larger.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      "http://example-1.com/",
      "http://example-2.com/",
    ]);
  }
  await PlacesTestUtils.addVisits("http://example-1.com/");

  // Wait for example-1.com to be listed second, after the Amazon search link.
  await updateTopSites(sites => {
    return sites && sites[1] && sites[1].url == "http://example-1.com/";
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.feeds.system.topsites", false]],
  });

  let expectedResults = [
    {
      type: "url",
      url: "http://example-1.com/",
      title: "test visit for http://example-1.com/",
      favicon: null,
    },
    {
      type: "url",
      url: "http://example-2.com/",
      title: "test visit for http://example-2.com/",
      favicon: null,
    },
  ];

  Assert.deepEqual(
    expectedResults,
    await getSites(extension, { newtab: true }),
    "Got top-frecency links from Places"
  );

  await SpecialPowers.popPrefEnv();
  await extension.unload();
  await PlacesUtils.history.clear();
});

// Tests newtab links with some visits and favicons.
add_task(async function test_topSites_newtab_visits_favicons() {
  let extension = await loadExtension();

  // Add some visits to a couple of URLs.  We need to add at least two visits
  // per URL for it to show up.  Add some extra to be safe, and add one more to
  // the first so that its frecency is larger.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      "http://example-1.com/",
      "http://example-2.com/",
    ]);
  }
  await PlacesTestUtils.addVisits("http://example-1.com/");

  // Give the first URL a favicon but not the second so that we can test links
  // both with and without favicons.
  let faviconData = new Map();
  faviconData.set("http://example-1.com", IMAGE_1x1);
  await PlacesTestUtils.addFavicons(faviconData);

  // Wait for example-1.com to be listed second, after the Amazon search link.
  await updateTopSites(sites => {
    return sites && sites[1] && sites[1].url == "http://example-1.com/";
  });

  let base = "chrome://activity-stream/content/data/content/tippytop/images/";

  let expectedResults = [
    {
      type: "search",
      url: "https://amazon.com",
      title: "@amazon",
      favicon: await makeDataURI(`${base}amazon@2x.png`),
    },
    {
      type: "url",
      url: "http://example-1.com/",
      title: "test visit for http://example-1.com/",
      favicon: IMAGE_1x1,
    },
    {
      type: "url",
      url: "http://example-2.com/",
      title: "test visit for http://example-2.com/",
      favicon: null,
    },
    {
      type: "url",
      url: "https://www.youtube.com/",
      title: "youtube",
      favicon: await makeDataURI(`${base}youtube-com@2x.png`),
    },
    {
      type: "url",
      url: "https://www.facebook.com/",
      title: "facebook",
      favicon: await makeDataURI(`${base}facebook-com@2x.png`),
    },
    {
      type: "url",
      url: "https://www.reddit.com/",
      title: "reddit",
      favicon: await makeDataURI(`${base}reddit-com@2x.png`),
    },
    {
      type: "url",
      url: "https://www.wikipedia.org/",
      title: "wikipedia",
      favicon: await makeDataURI(`${base}wikipedia-org@2x.png`),
    },
    {
      type: "url",
      url: "https://twitter.com/",
      title: "twitter",
      favicon: await makeDataURI(`${base}twitter-com@2x.png`),
    },
  ];

  Assert.deepEqual(
    expectedResults,
    await getSites(extension, { newtab: true, includeFavicon: true }),
    "got topSites newtab links"
  );

  await extension.unload();
  await PlacesUtils.history.clear();
});

// Tests newtab links with some visits, favicons, and the `limit` option.
add_task(async function test_topSites_newtab_visits_favicons_limit() {
  let extension = await loadExtension();

  // Add some visits to a couple of URLs.  We need to add at least two visits
  // per URL for it to show up.  Add some extra to be safe, and add one more to
  // the first so that its frecency is larger.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      "http://example-1.com/",
      "http://example-2.com/",
    ]);
  }
  await PlacesTestUtils.addVisits("http://example-1.com/");

  // Give the first URL a favicon but not the second so that we can test links
  // both with and without favicons.
  let faviconData = new Map();
  faviconData.set("http://example-1.com", IMAGE_1x1);
  await PlacesTestUtils.addFavicons(faviconData);

  // Wait for example-1.com to be listed second, after the Amazon search link.
  await updateTopSites(sites => {
    return sites && sites[1] && sites[1].url == "http://example-1.com/";
  });

  let expectedResults = [
    {
      type: "search",
      url: "https://amazon.com",
      title: "@amazon",
      favicon: await makeDataURI(
        "chrome://activity-stream/content/data/content/tippytop/images/amazon@2x.png"
      ),
    },
    {
      type: "url",
      url: "http://example-1.com/",
      title: "test visit for http://example-1.com/",
      favicon: IMAGE_1x1,
    },
    {
      type: "url",
      url: "http://example-2.com/",
      title: "test visit for http://example-2.com/",
      favicon: null,
    },
  ];

  Assert.deepEqual(
    expectedResults,
    await getSites(extension, { newtab: true, includeFavicon: true, limit: 3 }),
    "got topSites newtab links"
  );

  await extension.unload();
  await PlacesUtils.history.clear();
});
