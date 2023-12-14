/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TopSitesFeed, DEFAULT_TOP_SITES } = ChromeUtils.import(
  "resource://activity-stream/lib/TopSitesFeed.jsm"
);

const { actionCreators: ac, actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.sys.mjs"
);

const { Screenshots } = ChromeUtils.import(
  "resource://activity-stream/lib/Screenshots.jsm"
);
const { shortURL } = ChromeUtils.import(
  "resource://activity-stream/lib/ShortURL.jsm"
);
const { FilterAdult } = ChromeUtils.import(
  "resource://activity-stream/lib/FilterAdult.jsm"
);

ChromeUtils.defineESModuleGetters(this, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PageThumbs: "resource://gre/modules/PageThumbs.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  Sampling: "resource://gre/modules/components-utils/Sampling.sys.mjs",
  SearchService: "resource://gre/modules/SearchService.sys.mjs",
  TOP_SITES_DEFAULT_ROWS: "resource://activity-stream/common/Reducers.sys.mjs",
  TOP_SITES_MAX_SITES_PER_ROW:
    "resource://activity-stream/common/Reducers.sys.mjs",
});

const FAKE_FAVICON = "data987";
const FAKE_FAVICON_SIZE = 128;
const FAKE_FRECENCY = 200;
const FAKE_LINKS = new Array(2 * TOP_SITES_MAX_SITES_PER_ROW)
  .fill(null)
  .map((v, i) => ({
    frecency: FAKE_FRECENCY,
    url: `http://www.site${i}.com`,
  }));
const FAKE_SCREENSHOT = "data123";
const SEARCH_SHORTCUTS_EXPERIMENT_PREF = "improvesearch.topSiteSearchShortcuts";
const SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF =
  "improvesearch.topSiteSearchShortcuts.searchEngines";
const SEARCH_SHORTCUTS_HAVE_PINNED_PREF =
  "improvesearch.topSiteSearchShortcuts.havePinned";
const SHOWN_ON_NEWTAB_PREF = "feeds.topsites";
const SHOW_SPONSORED_PREF = "showSponsoredTopSites";
const TOP_SITES_BLOCKED_SPONSORS_PREF = "browser.topsites.blockedSponsors";
const CONTILE_CACHE_PREF = "browser.topsites.contile.cachedTiles";

// This pref controls how long the contile cache is valid for in seconds.
const CONTILE_CACHE_VALID_FOR_SECONDS_PREF =
  "browser.topsites.contile.cacheValidFor";
// This pref records when the last contile fetch occurred, as a UNIX timestamp
// in seconds.
const CONTILE_CACHE_LAST_FETCH_PREF = "browser.topsites.contile.lastFetch";

function FakeTippyTopProvider() {}
FakeTippyTopProvider.prototype = {
  async init() {
    this.initialized = true;
  },
  processSite(site) {
    return site;
  },
};

let gSearchServiceInitStub;
let gGetTopSitesStub;

function getTopSitesFeedForTest(sandbox) {
  let feed = new TopSitesFeed();
  const storage = {
    init: sandbox.stub().resolves(),
    get: sandbox.stub().resolves(),
    set: sandbox.stub().resolves(),
  };

  // Setup for tests that don't call `init` but require feed.storage
  feed._storage = storage;
  feed.store = {
    dispatch: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      Prefs: { values: { topSitesRows: 2 } },
      TopSites: { rows: Array(12).fill("site") },
    },
    dbStorage: { getDbTable: sandbox.stub().returns(storage) },
  };

  return feed;
}

add_setup(async () => {
  let sandbox = sinon.createSandbox();
  sandbox.stub(SearchService.prototype, "defaultEngine").get(() => {
    return { identifier: "ddg", searchForm: "https://duckduckgo.com" };
  });

  gGetTopSitesStub = sandbox
    .stub(NewTabUtils.activityStreamLinks, "getTopSites")
    .resolves(FAKE_LINKS);

  gSearchServiceInitStub = sandbox
    .stub(SearchService.prototype, "init")
    .resolves();

  sandbox.stub(NewTabUtils.activityStreamProvider, "_faviconBytesToDataURI");

  sandbox
    .stub(NewTabUtils.activityStreamProvider, "_addFavicons")
    .callsFake(l => {
      return Promise.resolve(
        l.map(link => {
          link.favicon = FAKE_FAVICON;
          link.faviconSize = FAKE_FAVICON_SIZE;
          return link;
        })
      );
    });

  sandbox.stub(Screenshots, "getScreenshotForURL").resolves(FAKE_SCREENSHOT);
  sandbox.spy(Screenshots, "maybeCacheScreenshot");
  sandbox.stub(Screenshots, "_shouldGetScreenshots").returns(true);

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(async function test_construction() {
  let feed = new TopSitesFeed();
  Assert.ok(feed, "Could construct a TopSitesFeed");
  Assert.ok(feed._currentSearchHostname, "_currentSearchHostname defined");
});

add_task(async function test_refreshDefaults() {
  let sandbox = sinon.createSandbox();
  let feed = new TopSitesFeed();
  Assert.ok(
    !DEFAULT_TOP_SITES.length,
    "Should have 0 DEFAULT_TOP_SITES initially."
  );

  info("refreshDefaults should add defaults on PREFS_INITIAL_VALUES");
  feed.onAction({
    type: at.PREFS_INITIAL_VALUES,
    data: { "default.sites": "https://foo.com" },
  });

  Assert.equal(
    DEFAULT_TOP_SITES.length,
    1,
    "Should have 1 DEFAULT_TOP_SITES now."
  );

  // Reset the DEFAULT_TOP_SITES;
  DEFAULT_TOP_SITES.length = 0;

  info("refreshDefaults should add defaults on default.sites PREF_CHANGED");
  feed.onAction({
    type: at.PREF_CHANGED,
    data: { name: "default.sites", value: "https://foo.com" },
  });

  Assert.equal(
    DEFAULT_TOP_SITES.length,
    1,
    "Should have 1 DEFAULT_TOP_SITES now."
  );

  // Reset the DEFAULT_TOP_SITES;
  DEFAULT_TOP_SITES.length = 0;

  info("refreshDefaults should refresh on topSiteRows PREF_CHANGED");
  let refreshStub = sandbox.stub(feed, "refresh");
  feed.onAction({ type: at.PREF_CHANGED, data: { name: "topSitesRows" } });
  Assert.ok(feed.refresh.calledOnce, "refresh called");
  refreshStub.restore();

  // Reset the DEFAULT_TOP_SITES;
  DEFAULT_TOP_SITES.length = 0;

  info("refreshDefaults should have default sites with .isDefault = true");
  feed.refreshDefaults("https://foo.com");
  Assert.equal(
    DEFAULT_TOP_SITES.length,
    1,
    "Should have a DEFAULT_TOP_SITES now."
  );
  Assert.ok(
    DEFAULT_TOP_SITES[0].isDefault,
    "Lone top site should be the default."
  );

  // Reset the DEFAULT_TOP_SITES;
  DEFAULT_TOP_SITES.length = 0;

  info("refreshDefaults should have default sites with appropriate hostname");
  feed.refreshDefaults("https://foo.com");
  Assert.equal(
    DEFAULT_TOP_SITES.length,
    1,
    "Should have a DEFAULT_TOP_SITES now."
  );
  let [site] = DEFAULT_TOP_SITES;
  Assert.equal(
    site.hostname,
    shortURL(site),
    "Lone top site should have the right hostname."
  );

  // Reset the DEFAULT_TOP_SITES;
  DEFAULT_TOP_SITES.length = 0;

  info("refreshDefaults should add no defaults on empty pref");
  feed.refreshDefaults("");
  Assert.equal(
    DEFAULT_TOP_SITES.length,
    0,
    "Should have 0 DEFAULT_TOP_SITES now."
  );

  info("refreshDefaults should be able to clear defaults");
  feed.refreshDefaults("https://foo.com");
  feed.refreshDefaults("");

  Assert.equal(
    DEFAULT_TOP_SITES.length,
    0,
    "Should have 0 DEFAULT_TOP_SITES now."
  );

  sandbox.restore();
});

add_task(async function test_filterForThumbnailExpiration() {
  let sandbox = sinon.createSandbox();
  let feed = getTopSitesFeedForTest(sandbox);

  info(
    "filterForThumbnailExpiration should pass rows.urls to the callback provided"
  );
  const rows = [
    { url: "foo.com" },
    { url: "bar.com", customScreenshotURL: "custom" },
  ];
  feed.store.state.TopSites = { rows };
  const stub = sandbox.stub();
  feed.filterForThumbnailExpiration(stub);
  Assert.ok(stub.calledOnce);
  Assert.ok(stub.calledWithExactly(["foo.com", "bar.com", "custom"]));

  sandbox.restore();
});

add_task(
  async function test_getLinksWithDefaults_on_SearchService_init_failure() {
    let sandbox = sinon.createSandbox();
    let feed = getTopSitesFeedForTest(sandbox);

    feed.refreshDefaults("https://foo.com");

    gSearchServiceInitStub.rejects(new Error("Simulating search init failure"));

    const result = await feed.getLinksWithDefaults();
    Assert.ok(result);

    gSearchServiceInitStub.resolves();

    sandbox.restore();
  }
);

add_task(async function test_getLinksWithDefaults() {
  NewTabUtils.activityStreamLinks.getTopSites.resetHistory();

  let sandbox = sinon.createSandbox();
  let feed = getTopSitesFeedForTest(sandbox);

  feed.refreshDefaults("https://foo.com");

  info("getLinksWithDefaults should get the links from NewTabUtils");
  let result = await feed.getLinksWithDefaults();

  const reference = FAKE_LINKS.map(site =>
    Object.assign({}, site, {
      hostname: shortURL(site),
      typedBonus: true,
    })
  );

  Assert.deepEqual(result, reference);
  Assert.ok(NewTabUtils.activityStreamLinks.getTopSites.calledOnce);

  info("getLinksWithDefaults should indicate the links get typed bonus");
  Assert.ok(result[0].typedBonus, "Expected typed bonus property to be true.");

  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_filterAdult() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should filter out non-pinned adult sites");

  sandbox.stub(FilterAdult, "filter").returns([]);
  const TEST_URL = "https://foo.com/";
  sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [{ url: TEST_URL }]);

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  const result = await feed.getLinksWithDefaults();
  Assert.ok(FilterAdult.filter.calledOnce);
  Assert.equal(result.length, 1);
  Assert.equal(result[0].url, TEST_URL);

  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_caching() {
  let sandbox = sinon.createSandbox();

  info(
    "getLinksWithDefaults should filter out the defaults that have been blocked"
  );
  // make sure we only have one top site, and we block the only default site we have to show
  const url = "www.myonlytopsite.com";
  const topsite = {
    frecency: FAKE_FRECENCY,
    hostname: shortURL({ url }),
    typedBonus: true,
    url,
  };

  const blockedDefaultSite = { url: "https://foo.com" };
  gGetTopSitesStub.resolves([topsite]);
  sandbox.stub(NewTabUtils.blockedLinks, "isBlocked").callsFake(site => {
    return site.url === blockedDefaultSite.url;
  });

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");
  const result = await feed.getLinksWithDefaults();

  // what we should be left with is just the top site we added, and not the default site we blocked
  Assert.equal(result.length, 1);
  Assert.deepEqual(result[0], topsite);
  let foundBlocked = result.find(site => site.url === blockedDefaultSite.url);
  Assert.ok(!foundBlocked, "Should not have found blocked site.");

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_dedupe() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should call dedupe.group on the links");
  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  let stub = sandbox.stub(feed.dedupe, "group").callsFake((...id) => id);
  await feed.getLinksWithDefaults();

  Assert.ok(stub.calledOnce, "dedupe.group was called once");
  sandbox.restore();
});

add_task(async function test__dedupe_key() {
  let sandbox = sinon.createSandbox();

  info("_dedupeKey should dedupe on hostname instead of url");
  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  let site = { url: "foo", hostname: "bar" };
  let result = feed._dedupeKey(site);

  Assert.equal(result, site.hostname, "deduped on hostname");
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_adds_defaults() {
  let sandbox = sinon.createSandbox();

  info(
    "getLinksWithDefaults should add defaults if there are are not enough links"
  );
  const TEST_LINKS = [{ frecency: FAKE_FRECENCY, url: "foo.com" }];
  gGetTopSitesStub.resolves(TEST_LINKS);
  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  let result = await feed.getLinksWithDefaults();

  let reference = [...TEST_LINKS, ...DEFAULT_TOP_SITES].map(s =>
    Object.assign({}, s, {
      hostname: shortURL(s),
      typedBonus: true,
    })
  );

  Assert.deepEqual(result, reference);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(
  async function test_getLinksWithDefaults_adds_defaults_for_visible_slots() {
    let sandbox = sinon.createSandbox();

    info(
      "getLinksWithDefaults should only add defaults up to the number of visible slots"
    );
    const numVisible = TOP_SITES_DEFAULT_ROWS * TOP_SITES_MAX_SITES_PER_ROW;
    let testLinks = [];
    for (let i = 0; i < numVisible - 1; i++) {
      testLinks.push({ frecency: FAKE_FRECENCY, url: `foo${i}.com` });
    }
    gGetTopSitesStub.resolves(testLinks);

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");

    let result = await feed.getLinksWithDefaults();

    let reference = [...testLinks, DEFAULT_TOP_SITES[0]].map(s =>
      Object.assign({}, s, {
        hostname: shortURL(s),
        typedBonus: true,
      })
    );

    Assert.equal(result.length, numVisible);
    Assert.deepEqual(result, reference);

    gGetTopSitesStub.resolves(FAKE_LINKS);
    sandbox.restore();
  }
);

add_task(async function test_getLinksWithDefaults_no_throw_on_no_links() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should not throw if NewTabUtils returns null");
  gGetTopSitesStub.resolves(null);

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  feed.getLinksWithDefaults();
  Assert.ok(true, "getLinksWithDefaults did not throw");

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_get_more_on_request() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should get more if the user has asked for more");
  let testLinks = new Array(4 * TOP_SITES_MAX_SITES_PER_ROW)
    .fill(null)
    .map((v, i) => ({
      frecency: FAKE_FRECENCY,
      url: `http://www.site${i}.com`,
    }));
  gGetTopSitesStub.resolves(testLinks);

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  const TEST_ROWS = 3;
  feed.store.state.Prefs.values.topSitesRows = TEST_ROWS;

  let result = await feed.getLinksWithDefaults();
  Assert.equal(result.length, TEST_ROWS * TOP_SITES_MAX_SITES_PER_ROW);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_reuse_cache() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should reuse the cache on subsequent calls");

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  gGetTopSitesStub.resetHistory();

  await feed.getLinksWithDefaults();
  await feed.getLinksWithDefaults();

  Assert.ok(
    NewTabUtils.activityStreamLinks.getTopSites.calledOnce,
    "getTopSites only called once"
  );

  sandbox.restore();
});

add_task(
  async function test_getLinksWithDefaults_ignore_cache_on_requesting_more() {
    let sandbox = sinon.createSandbox();
    info("getLinksWithDefaults should ignore the cache when requesting more");

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");

    gGetTopSitesStub.resetHistory();

    await feed.getLinksWithDefaults();
    feed.store.state.Prefs.values.topSitesRows *= 3;
    await feed.getLinksWithDefaults();

    Assert.ok(
      NewTabUtils.activityStreamLinks.getTopSites.calledTwice,
      "getTopSites called twice"
    );

    sandbox.restore();
  }
);

add_task(
  async function test_getLinksWithDefaults_migrate_frecent_screenshot_data() {
    let sandbox = sinon.createSandbox();
    info(
      "getLinksWithDefaults should migrate frecent screenshot data without getting screenshots again"
    );

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");

    gGetTopSitesStub.resetHistory();

    feed.store.state.Prefs.values[SHOWN_ON_NEWTAB_PREF] = true;
    await feed.getLinksWithDefaults();

    let originalCallCount = Screenshots.getScreenshotForURL.callCount;
    feed.frecentCache.expire();

    let result = await feed.getLinksWithDefaults();

    Assert.ok(
      NewTabUtils.activityStreamLinks.getTopSites.calledTwice,
      "getTopSites called twice"
    );
    Assert.equal(
      Screenshots.getScreenshotForURL.callCount,
      originalCallCount,
      "getScreenshotForURL was not called again."
    );
    Assert.equal(result[0].screenshot, FAKE_SCREENSHOT);

    sandbox.restore();
  }
);

add_task(
  async function test_getLinksWithDefaults_migrate_pinned_favicon_data() {
    let sandbox = sinon.createSandbox();
    info(
      "getLinksWithDefaults should migrate pinned favicon data without getting favicons again"
    );

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");

    gGetTopSitesStub.resetHistory();

    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "https://foo.com/" }]);

    await feed.getLinksWithDefaults();

    let originalCallCount =
      NewTabUtils.activityStreamProvider._addFavicons.callCount;
    feed.pinnedCache.expire();

    let result = await feed.getLinksWithDefaults();

    Assert.equal(
      NewTabUtils.activityStreamProvider._addFavicons.callCount,
      originalCallCount,
      "_addFavicons was not called again."
    );
    Assert.equal(result[0].favicon, FAKE_FAVICON);
    Assert.equal(result[0].faviconSize, FAKE_FAVICON_SIZE);

    sandbox.restore();
  }
);

add_task(async function test_getLinksWithDefaults_no_internal_properties() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should not expose internal link properties");

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  let result = await feed.getLinksWithDefaults();

  let internal = Object.keys(result[0]).filter(key => key.startsWith("__"));
  Assert.equal(internal.join(""), "");

  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_copy_frecent_screenshot() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should copy the screenshot of the frecent site if " +
      "pinned site doesn't have customScreenshotURL"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  const TEST_SCREENSHOT = "screenshot";

  gGetTopSitesStub.resolves([
    { url: "https://foo.com/", screenshot: TEST_SCREENSHOT },
  ]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com/" }]);

  let result = await feed.getLinksWithDefaults();

  Assert.equal(result[0].screenshot, TEST_SCREENSHOT);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_no_copy_frecent_screenshot() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should not copy the frecent screenshot if " +
      "customScreenshotURL is set"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  gGetTopSitesStub.resolves([
    { url: "https://foo.com/", screenshot: "screenshot" },
  ]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com/", customScreenshotURL: "custom" }]);

  let result = await feed.getLinksWithDefaults();

  Assert.equal(result[0].screenshot, undefined);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_persist_screenshot() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should keep the same screenshot if no frecent site is found"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  const CUSTOM_SCREENSHOT = "custom";

  gGetTopSitesStub.resolves([]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com/", screenshot: CUSTOM_SCREENSHOT }]);

  let result = await feed.getLinksWithDefaults();

  Assert.equal(result[0].screenshot, CUSTOM_SCREENSHOT);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(
  async function test_getLinksWithDefaults_no_overwrite_pinned_screenshot() {
    let sandbox = sinon.createSandbox();
    info("getLinksWithDefaults should not overwrite pinned site screenshot");

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");

    const EXISTING_SCREENSHOT = "some-screenshot";

    gGetTopSitesStub.resolves([{ url: "https://foo.com/", screenshot: "foo" }]);
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [
        { url: "https://foo.com/", screenshot: EXISTING_SCREENSHOT },
      ]);

    let result = await feed.getLinksWithDefaults();

    Assert.equal(result[0].screenshot, EXISTING_SCREENSHOT);

    gGetTopSitesStub.resolves(FAKE_LINKS);
    sandbox.restore();
  }
);

add_task(
  async function test_getLinksWithDefaults_no_searchTopSite_from_frecent() {
    let sandbox = sinon.createSandbox();
    info("getLinksWithDefaults should not set searchTopSite from frecent site");

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");

    const EXISTING_SCREENSHOT = "some-screenshot";

    gGetTopSitesStub.resolves([
      {
        url: "https://foo.com/",
        searchTopSite: true,
        screenshot: EXISTING_SCREENSHOT,
      },
    ]);
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "https://foo.com/" }]);

    let result = await feed.getLinksWithDefaults();

    Assert.ok(!result[0].searchTopSite);
    // But it should copy over other properties
    Assert.equal(result[0].screenshot, EXISTING_SCREENSHOT);

    gGetTopSitesStub.resolves(FAKE_LINKS);
    sandbox.restore();
  }
);

add_task(async function test_getLinksWithDefaults_concurrency_getTopSites() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults concurrent calls should call the backing data once"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  NewTabUtils.activityStreamLinks.getTopSites.resetHistory();

  await Promise.all([feed.getLinksWithDefaults(), feed.getLinksWithDefaults()]);

  Assert.ok(
    NewTabUtils.activityStreamLinks.getTopSites.calledOnce,
    "getTopSites only called once"
  );

  sandbox.restore();
});

add_task(
  async function test_getLinksWithDefaults_concurrency_getScreenshotForURL() {
    let sandbox = sinon.createSandbox();
    info(
      "getLinksWithDefaults concurrent calls should call the backing data once"
    );

    let feed = getTopSitesFeedForTest(sandbox);
    feed.refreshDefaults("https://foo.com");
    feed.store.state.Prefs.values[SHOWN_ON_NEWTAB_PREF] = true;

    NewTabUtils.activityStreamLinks.getTopSites.resetHistory();
    Screenshots.getScreenshotForURL.resetHistory();

    await Promise.all([
      feed.getLinksWithDefaults(),
      feed.getLinksWithDefaults(),
    ]);

    Assert.ok(
      NewTabUtils.activityStreamLinks.getTopSites.calledOnce,
      "getTopSites only called once"
    );

    Assert.equal(
      Screenshots.getScreenshotForURL.callCount,
      FAKE_LINKS.length,
      "getLinksWithDefaults concurrent calls should get screenshots once per link"
    );

    feed = getTopSitesFeedForTest(sandbox);
    feed.store.state.Prefs.values[SHOWN_ON_NEWTAB_PREF] = true;

    feed.refreshDefaults("https://foo.com");

    sandbox.stub(feed, "_requestRichIcon");
    await Promise.all([
      feed.getLinksWithDefaults(),
      feed.getLinksWithDefaults(),
    ]);

    Assert.equal(
      feed.store.dispatch.callCount,
      FAKE_LINKS.length,
      "getLinksWithDefaults concurrent calls should dispatch once per link screenshot fetched"
    );

    sandbox.restore();
  }
);

add_task(async function test_getLinksWithDefaults_deduping_no_dedupe_pinned() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should not dedupe pinned sites");

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults("https://foo.com");

  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [
      { url: "https://developer.mozilla.org/en-US/docs/Web" },
      { url: "https://developer.mozilla.org/en-US/docs/Learn" },
    ]);

  let sites = await feed.getLinksWithDefaults();
  Assert.equal(sites.length, 2 * TOP_SITES_MAX_SITES_PER_ROW);
  Assert.equal(sites[0].url, NewTabUtils.pinnedLinks.links[0].url);
  Assert.equal(sites[1].url, NewTabUtils.pinnedLinks.links[1].url);
  Assert.equal(sites[0].hostname, sites[1].hostname);

  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_prefer_pinned_sites() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should prefer pinned sites over links");

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults();

  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [
      { url: "https://developer.mozilla.org/en-US/docs/Web" },
      { url: "https://developer.mozilla.org/en-US/docs/Learn" },
    ]);

  const SECOND_TOP_SITE_URL = "https://www.mozilla.org/";

  gGetTopSitesStub.resolves([
    { frecency: FAKE_FRECENCY, url: "https://developer.mozilla.org/" },
    { frecency: FAKE_FRECENCY, url: SECOND_TOP_SITE_URL },
  ]);

  let sites = await feed.getLinksWithDefaults();

  // Expecting 3 links where there's 2 pinned and 1 www.mozilla.org, so
  // the frecent with matching hostname as pinned is removed.
  Assert.equal(sites.length, 3);
  Assert.equal(sites[0].url, NewTabUtils.pinnedLinks.links[0].url);
  Assert.equal(sites[1].url, NewTabUtils.pinnedLinks.links[1].url);
  Assert.equal(sites[2].url, SECOND_TOP_SITE_URL);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_title_and_null() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should return sites that have a title");

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults();

  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://github.com/mozilla/activity-stream" }]);

  let sites = await feed.getLinksWithDefaults();
  for (let site of sites) {
    Assert.ok(site.hostname);
  }

  info("getLinksWithDefaults should not throw for null entries");
  sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [null]);
  await feed.getLinksWithDefaults();
  Assert.ok(true, "getLinksWithDefaults didn't throw");

  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_calls__fetchIcon() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should return sites that have a title");

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults();

  sandbox.spy(feed, "_fetchIcon");
  let results = await feed.getLinksWithDefaults();
  Assert.ok(results.length, "Got back some results");
  Assert.equal(feed._fetchIcon.callCount, results.length);
  for (let result of results) {
    Assert.ok(feed._fetchIcon.calledWith(result));
  }

  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_calls__fetchScreenshot() {
  let sandbox = sinon.createSandbox();

  info(
    "getLinksWithDefaults should call _fetchScreenshot when customScreenshotURL is set"
  );

  gGetTopSitesStub.resolves([]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com", customScreenshotURL: "custom" }]);

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults();

  sandbox.stub(feed, "_fetchScreenshot");
  await feed.getLinksWithDefaults();

  Assert.ok(feed._fetchScreenshot.calledWith(sinon.match.object, "custom"));

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_getLinksWithDefaults_with_DiscoveryStream() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should add a sponsored topsite from discoverystream to all the valid indices"
  );

  let makeStreamData = index => ({
    layout: [
      {
        components: [
          {
            placement: {
              name: "sponsored-topsites",
            },
            spocs: {
              positions: [{ index }],
            },
          },
        ],
      },
    ],
    spocs: {
      data: {
        "sponsored-topsites": {
          items: [{ title: "test spoc", url: "https://test-spoc.com" }],
        },
      },
    },
  });

  let feed = getTopSitesFeedForTest(sandbox);
  feed.refreshDefaults();

  for (let i = 0; i < FAKE_LINKS.length; i++) {
    feed.store.state.DiscoveryStream = makeStreamData(i);
    const result = await feed.getLinksWithDefaults();
    const link = result[i];

    Assert.equal(link.type, "SPOC");
    Assert.equal(link.title, "test spoc");
    Assert.equal(link.sponsored_position, i + 1);
    Assert.equal(link.hostname, "test-spoc");
    Assert.equal(link.url, "https://test-spoc.com");
  }

  sandbox.restore();
});

add_task(async function test_init() {
  let sandbox = sinon.createSandbox();

  sandbox.stub(NimbusFeatures.newtab, "onUpdate");

  let feed = getTopSitesFeedForTest(sandbox);

  sandbox.stub(feed, "refresh");
  await feed.init();

  info("TopSitesFeed.init should call refresh (broadcast: true)");
  Assert.ok(feed.refresh.calledOnce, "refresh called once");
  Assert.ok(
    feed.refresh.calledWithExactly({
      broadcast: true,
      isStartup: true,
    })
  );

  info("TopSitesFeed.init should initialise the storage");
  Assert.ok(
    feed.store.dbStorage.getDbTable.calledOnce,
    "getDbTable called once"
  );
  Assert.ok(feed.store.dbStorage.getDbTable.calledWithExactly("sectionPrefs"));

  info(
    "TopSitesFeed.init should call onUpdate to set up Nimbus update listener"
  );

  Assert.ok(
    NimbusFeatures.newtab.onUpdate.calledOnce,
    "NimbusFeatures.newtab.onUpdate called once"
  );
  sandbox.restore();
});

add_task(async function test_refresh() {
  let sandbox = sinon.createSandbox();

  sandbox.stub(NimbusFeatures.newtab, "onUpdate");

  let feed = getTopSitesFeedForTest(sandbox);

  sandbox.stub(feed, "_fetchIcon");
  feed._startedUp = true;

  info("TopSitesFeed.refresh should wait for tippytop to initialize");
  feed._tippyTopProvider.initialized = false;
  sandbox.stub(feed._tippyTopProvider, "init").resolves();

  await feed.refresh();

  Assert.ok(
    feed._tippyTopProvider.init.calledOnce,
    "feed._tippyTopProvider.init called once"
  );

  info(
    "TopSitesFeed.refresh should not init the tippyTopProvider if already initialized"
  );
  feed._tippyTopProvider.initialized = true;
  feed._tippyTopProvider.init.resetHistory();

  await feed.refresh();

  Assert.ok(
    feed._tippyTopProvider.init.notCalled,
    "tippyTopProvider not initted again"
  );

  info("TopSitesFeed.refresh should broadcast TOP_SITES_UPDATED");
  feed.store.dispatch.resetHistory();
  sandbox.stub(feed, "getLinksWithDefaults").resolves([]);

  await feed.refresh({ broadcast: true });

  Assert.ok(feed.store.dispatch.calledOnce, "dispatch called once");
  Assert.ok(
    feed.store.dispatch.calledWithExactly(
      ac.BroadcastToContent({
        type: at.TOP_SITES_UPDATED,
        data: { links: [], pref: { collapsed: false } },
      })
    )
  );

  sandbox.restore();
});

add_task(async function test_refresh_dispatch() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.refresh should dispatch an action with the links returned"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "_fetchIcon");
  feed._startedUp = true;

  await feed.refresh({ broadcast: true });
  let reference = FAKE_LINKS.map(site =>
    Object.assign({}, site, {
      hostname: shortURL(site),
      typedBonus: true,
    })
  );

  Assert.ok(feed.store.dispatch.calledOnce, "Store.dispatch called once");
  Assert.equal(
    feed.store.dispatch.firstCall.args[0].type,
    at.TOP_SITES_UPDATED
  );
  Assert.deepEqual(feed.store.dispatch.firstCall.args[0].data.links, reference);

  sandbox.restore();
});

add_task(async function test_refresh_empty_slots() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.refresh should handle empty slots in the resulting top sites array"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "_fetchIcon");
  feed._startedUp = true;

  gGetTopSitesStub.resolves([FAKE_LINKS[0]]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [
      null,
      null,
      FAKE_LINKS[1],
      null,
      null,
      null,
      null,
      null,
      FAKE_LINKS[2],
    ]);

  await feed.refresh({ broadcast: true });

  Assert.ok(feed.store.dispatch.calledOnce, "Store.dispatch called once");

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_refresh_to_preloaded() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.refresh should dispatch AlsoToPreloaded when broadcast is false"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "_fetchIcon");
  feed._startedUp = true;

  gGetTopSitesStub.resolves([]);
  await feed.refresh({ broadcast: false });

  Assert.ok(feed.store.dispatch.calledOnce, "Store.dispatch called once");
  Assert.ok(
    feed.store.dispatch.calledWithExactly(
      ac.AlsoToPreloaded({
        type: at.TOP_SITES_UPDATED,
        data: { links: [], pref: { collapsed: false } },
      })
    )
  );
  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
});

add_task(async function test_refresh_init_storage() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.refresh should not init storage of it's already initialized"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "_fetchIcon");
  feed._startedUp = true;

  feed._storage.initialized = true;

  await feed.refresh({ broadcast: false });

  Assert.ok(feed._storage.init.notCalled, "feed._storage.init was not called.");
  sandbox.restore();
});

add_task(async function test_refresh_handles_indexedDB_errors() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.refresh should dispatch AlsoToPreloaded when broadcast is false"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "_fetchIcon");
  feed._startedUp = true;

  feed._storage.get.throws(new Error());

  try {
    await feed.refresh({ broadcast: false });
    Assert.ok(true, "refresh should have succeeded");
  } catch (e) {
    Assert.ok(false, "Should not have thrown");
  }

  sandbox.restore();
});

add_task(async function test_updateSectionPrefs_on_UPDATE_SECTION_PREFS() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.onAction should call updateSectionPrefs on UPDATE_SECTION_PREFS"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "updateSectionPrefs");
  feed.onAction({
    type: at.UPDATE_SECTION_PREFS,
    data: { id: "topsites" },
  });

  Assert.ok(
    feed.updateSectionPrefs.calledOnce,
    "feed.updateSectionPrefs called once"
  );

  sandbox.restore();
});

add_task(
  async function test_updateSectionPrefs_dispatch_TOP_SITES_PREFS_UPDATED() {
    let sandbox = sinon.createSandbox();

    info(
      "TopSitesFeed.updateSectionPrefs should dispatch TOP_SITES_PREFS_UPDATED"
    );

    let feed = getTopSitesFeedForTest(sandbox);
    await feed.updateSectionPrefs({ collapsed: true });
    Assert.ok(
      feed.store.dispatch.calledWithExactly(
        ac.BroadcastToContent({
          type: at.TOP_SITES_PREFS_UPDATED,
          data: { pref: { collapsed: true } },
        })
      )
    );

    sandbox.restore();
  }
);

add_task(async function test_allocatePositions() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.allocationPositions should allocate positions and dispatch"
  );

  let feed = getTopSitesFeedForTest(sandbox);

  let sov = {
    name: "SOV-20230518215316",
    allocations: [
      {
        position: 1,
        allocation: [
          {
            partner: "amp",
            percentage: 100,
          },
          {
            partner: "moz-sales",
            percentage: 0,
          },
        ],
      },
      {
        position: 2,
        allocation: [
          {
            partner: "amp",
            percentage: 80,
          },
          {
            partner: "moz-sales",
            percentage: 20,
          },
        ],
      },
    ],
  };

  sandbox.stub(feed._contile, "sov").get(() => sov);

  sandbox.stub(Sampling, "ratioSample");
  Sampling.ratioSample.onCall(0).resolves(0);
  Sampling.ratioSample.onCall(1).resolves(1);

  await feed.allocatePositions();

  Assert.ok(feed.store.dispatch.calledOnce, "feed.store.dispatch called once");
  Assert.ok(
    feed.store.dispatch.calledWithExactly(
      ac.OnlyToMain({
        type: at.SOV_UPDATED,
        data: {
          ready: true,
          positions: [
            { position: 1, assignedPartner: "amp" },
            { position: 2, assignedPartner: "moz-sales" },
          ],
        },
      })
    )
  );

  Sampling.ratioSample.onCall(2).resolves(0);
  Sampling.ratioSample.onCall(3).resolves(0);

  await feed.allocatePositions();

  Assert.ok(
    feed.store.dispatch.calledTwice,
    "feed.store.dispatch called twice"
  );
  Assert.ok(
    feed.store.dispatch.calledWithExactly(
      ac.OnlyToMain({
        type: at.SOV_UPDATED,
        data: {
          ready: true,
          positions: [
            { position: 1, assignedPartner: "amp" },
            { position: 2, assignedPartner: "amp" },
          ],
        },
      })
    )
  );

  sandbox.restore();
});

add_task(async function test_getScreenshotPreview() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.getScreenshotPreview should dispatch preview if request is succesful"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  await feed.getScreenshotPreview("custom", 1234);

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWithExactly(
      ac.OnlyToOneContent(
        {
          data: { preview: FAKE_SCREENSHOT, url: "custom" },
          type: at.PREVIEW_RESPONSE,
        },
        1234
      )
    )
  );

  sandbox.restore();
});

add_task(async function test_getScreenshotPreview() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.getScreenshotPreview should return empty string if request fails"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  Screenshots.getScreenshotForURL.resolves(Promise.resolve(null));
  await feed.getScreenshotPreview("custom", 1234);

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWithExactly(
      ac.OnlyToOneContent(
        {
          data: { preview: "", url: "custom" },
          type: at.PREVIEW_RESPONSE,
        },
        1234
      )
    )
  );

  Screenshots.getScreenshotForURL.resolves(FAKE_SCREENSHOT);
  sandbox.restore();
});

add_task(async function test_onAction_part_1() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.onAction should call getScreenshotPreview on PREVIEW_REQUEST"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "getScreenshotPreview");

  feed.onAction({
    type: at.PREVIEW_REQUEST,
    data: { url: "foo" },
    meta: { fromTarget: 1234 },
  });

  Assert.ok(
    feed.getScreenshotPreview.calledOnce,
    "feed.getScreenshotPreview called once"
  );
  Assert.ok(feed.getScreenshotPreview.calledWithExactly("foo", 1234));

  info("TopSitesFeed.onAction should refresh on SYSTEM_TICK");
  sandbox.stub(feed, "refresh");
  feed.onAction({ type: at.SYSTEM_TICK });

  Assert.ok(feed.refresh.calledOnce, "feed.refresh called once");
  Assert.ok(feed.refresh.calledWithExactly({ broadcast: false }));

  info(
    "TopSitesFeed.onAction should call with correct parameters on TOP_SITES_PIN"
  );
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  sandbox.spy(feed, "pin");

  let pinAction = {
    type: at.TOP_SITES_PIN,
    data: { site: { url: "foo.com" }, index: 7 },
  };
  feed.onAction(pinAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.pin.calledOnce,
    "NewTabUtils.pinnedLinks.pin called once"
  );
  Assert.ok(
    NewTabUtils.pinnedLinks.pin.calledWithExactly(
      pinAction.data.site,
      pinAction.data.index
    )
  );
  Assert.ok(
    feed.pin.calledOnce,
    "TopSitesFeed.onAction should call pin on TOP_SITES_PIN"
  );

  info(
    "TopSitesFeed.onAction should unblock a previously blocked top site if " +
      "we are now adding it manually via 'Add a Top Site' option"
  );
  sandbox.stub(NewTabUtils.blockedLinks, "unblock");
  pinAction = {
    type: at.TOP_SITES_PIN,
    data: { site: { url: "foo.com" }, index: -1 },
  };
  feed.onAction(pinAction);
  Assert.ok(
    NewTabUtils.blockedLinks.unblock.calledWith({
      url: pinAction.data.site.url,
    })
  );

  info("TopSitesFeed.onAction should call insert on TOP_SITES_INSERT");
  sandbox.stub(feed, "insert");
  let addAction = {
    type: at.TOP_SITES_INSERT,
    data: { site: { url: "foo.com" } },
  };

  feed.onAction(addAction);
  Assert.ok(feed.insert.calledOnce, "TopSitesFeed.insert called once");

  info(
    "TopSitesFeed.onAction should call unpin with correct parameters " +
      "on TOP_SITES_UNPIN"
  );

  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [
      null,
      null,
      { url: "foo.com" },
      null,
      null,
      null,
      null,
      null,
      FAKE_LINKS[0],
    ]);
  sandbox.stub(NewTabUtils.pinnedLinks, "unpin");

  let unpinAction = {
    type: at.TOP_SITES_UNPIN,
    data: { site: { url: "foo.com" } },
  };
  feed.onAction(unpinAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.unpin.calledOnce,
    "NewTabUtils.pinnedLinks.unpin called once"
  );
  Assert.ok(NewTabUtils.pinnedLinks.unpin.calledWith(unpinAction.data.site));

  sandbox.restore();
});

add_task(async function test_onAction_part_2() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSitesFeed.onAction should call refresh without a target if we clear " +
      "history with PLACES_HISTORY_CLEARED"
  );

  let feed = getTopSitesFeedForTest(sandbox);
  sandbox.stub(feed, "refresh");
  feed.onAction({ type: at.PLACES_HISTORY_CLEARED });

  Assert.ok(feed.refresh.calledOnce, "TopSitesFeed.refresh called once");
  Assert.ok(feed.refresh.calledWithExactly({ broadcast: true }));

  feed.refresh.resetHistory();

  info(
    "TopSitesFeed.onAction should call refresh without a target " +
      "if we remove a Topsite from history"
  );
  feed.onAction({ type: at.PLACES_LINKS_DELETED });

  Assert.ok(feed.refresh.calledOnce, "TopSitesFeed.refresh called once");
  Assert.ok(feed.refresh.calledWithExactly({ broadcast: true }));

  info("TopSitesFeed.onAction should call init on INIT action");
  feed.onAction({ type: at.PLACES_LINKS_DELETED });
  sandbox.stub(feed, "init");
  feed.onAction({ type: at.INIT });
  Assert.ok(feed.init.calledOnce, "TopSitesFeed.init called once");

  info(
    "TopSitesFeed.onAction should call refresh on PLACES_LINK_BLOCKED action"
  );
  feed.refresh.resetHistory();
  await feed.onAction({ type: at.PLACES_LINK_BLOCKED });
  Assert.ok(feed.refresh.calledOnce, "TopSitesFeed.refresh called once");
  Assert.ok(feed.refresh.calledWithExactly({ broadcast: true }));

  info(
    "TopSitesFeed.onAction should call refresh on PLACES_LINKS_CHANGED action"
  );
  feed.refresh.resetHistory();
  await feed.onAction({ type: at.PLACES_LINKS_CHANGED });
  Assert.ok(feed.refresh.calledOnce, "TopSitesFeed.refresh called once");
  Assert.ok(feed.refresh.calledWithExactly({ broadcast: false }));

  info(
    "TopSitesFeed.onAction should call pin with correct args on " +
      "TOP_SITES_INSERT without an index specified"
  );
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  let addAction = {
    type: at.TOP_SITES_INSERT,
    data: { site: { url: "foo.bar", label: "foo" } },
  };
  feed.onAction(addAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.pin.calledOnce,
    "NewTabUtils.pinnedLinks.pin called once"
  );
  Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(addAction.data.site, 0));

  info(
    "TopSitesFeed.onAction should call pin with correct args on " +
      "TOP_SITES_INSERT"
  );
  NewTabUtils.pinnedLinks.pin.resetHistory();
  let dropAction = {
    type: at.TOP_SITES_INSERT,
    data: { site: { url: "foo.bar", label: "foo" }, index: 3 },
  };
  feed.onAction(dropAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.pin.calledOnce,
    "NewTabUtils.pinnedLinks.pin called once"
  );
  Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(dropAction.data.site, 3));

  // feed.init needs to actually run in order to register the observers that'll
  // be removed in the following UNINIT test, otherwise uninit will throw.
  feed.init.restore();
  feed.init();

  info("TopSitesFeed.onAction should remove the expiration filter on UNINIT");
  sandbox.stub(PageThumbs, "removeExpirationFilter");
  feed.onAction({ type: "UNINIT" });
  Assert.ok(
    PageThumbs.removeExpirationFilter.calledOnce,
    "PageThumbs.removeExpirationFilter called once"
  );

  sandbox.restore();
});

add_task(async function test_onAction_part_3() {
  let sandbox = sinon.createSandbox();

  let feed = getTopSitesFeedForTest(sandbox);

  info(
    "TopSitesFeed.onAction should call updatePinnedSearchShortcuts " +
      "on UPDATE_PINNED_SEARCH_SHORTCUTS action"
  );
  sandbox.stub(feed, "updatePinnedSearchShortcuts");
  let addedShortcuts = [
    {
      url: "https://google.com",
      searchVendor: "google",
      label: "google",
      searchTopSite: true,
    },
  ];
  await feed.onAction({
    type: at.UPDATE_PINNED_SEARCH_SHORTCUTS,
    data: { addedShortcuts },
  });
  Assert.ok(
    feed.updatePinnedSearchShortcuts.calledOnce,
    "TopSitesFeed.updatePinnedSearchShortcuts called once"
  );

  info(
    "TopSitesFeed.onAction should refresh from Contile on " +
      "SHOW_SPONSORED_PREF if Contile is enabled"
  );
  sandbox.spy(feed._contile, "refresh");
  let prefChangeAction = {
    type: at.PREF_CHANGED,
    data: { name: SHOW_SPONSORED_PREF },
  };
  sandbox.stub(NimbusFeatures.newtab, "getVariable").returns(true);
  feed.onAction(prefChangeAction);

  Assert.ok(
    feed._contile.refresh.calledOnce,
    "TopSitesFeed._contile.refresh called once"
  );

  info(
    "TopSitesFeed.onAction should not refresh from Contile on " +
      "SHOW_SPONSORED_PREF if Contile is disabled"
  );
  NimbusFeatures.newtab.getVariable.returns(false);
  feed._contile.refresh.resetHistory();
  feed.onAction(prefChangeAction);

  Assert.ok(
    !feed._contile.refresh.calledOnce,
    "TopSitesFeed._contile.refresh never called"
  );

  info(
    "TopSitesFeed.onAction should reset Contile cache prefs " +
      "when SHOW_SPONSORED_PREF is false"
  );
  Services.prefs.setStringPref(CONTILE_CACHE_PREF, "[]");
  Services.prefs.setIntPref(
    CONTILE_CACHE_LAST_FETCH_PREF,
    Math.round(Date.now() / 1000)
  );
  Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_SECONDS_PREF, 15 * 60);
  prefChangeAction = {
    type: at.PREF_CHANGED,
    data: { name: SHOW_SPONSORED_PREF, value: false },
  };
  NimbusFeatures.newtab.getVariable.returns(true);
  feed._contile.refresh.resetHistory();

  feed.onAction(prefChangeAction);
  Assert.ok(!Services.prefs.prefHasUserValue(CONTILE_CACHE_PREF));
  Assert.ok(!Services.prefs.prefHasUserValue(CONTILE_CACHE_LAST_FETCH_PREF));
  Assert.ok(
    !Services.prefs.prefHasUserValue(CONTILE_CACHE_VALID_FOR_SECONDS_PREF)
  );

  sandbox.restore();
});

add_task(async function test_insert_part_1() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info(
      "TopSitesFeed.insert should pin site in first slot of empty pinned list"
    );

    let feed = getTopSitesFeedForTest(sandbox);
    Screenshots.getScreenshotForURL.resolves(Promise.resolve(null));
    await feed.getScreenshotPreview("custom", 1234);

    Assert.ok(feed.store.dispatch.calledOnce);
    Assert.ok(
      feed.store.dispatch.calledWithExactly(
        ac.OnlyToOneContent(
          {
            data: { preview: "", url: "custom" },
            type: at.PREVIEW_RESPONSE,
          },
          1234
        )
      )
    );

    Screenshots.getScreenshotForURL.resolves(FAKE_SCREENSHOT);
  }

  {
    info(
      "TopSitesFeed.insert should pin site in first slot of pinned list with " +
        "empty first slot"
    );

    let feed = getTopSitesFeedForTest(sandbox);
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);
    let site = { url: "foo.bar", label: "foo" };
    await feed.insert({ data: { site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.insert should move a pinned site in first slot to the " +
        "next slot: part 1"
    );
    let site1 = { url: "example.com" };
    sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [site1]);
    let feed = getTopSitesFeedForTest(sandbox);
    let site = { url: "foo.bar", label: "foo" };

    await feed.insert({ data: { site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 1));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.insert should move a pinned site in first slot to the " +
        "next slot: part 2"
    );
    let site1 = { url: "example.com" };
    let site2 = { url: "example.org" };
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [site1, null, site2]);
    let site = { url: "foo.bar", label: "foo" };
    let feed = getTopSitesFeedForTest(sandbox);
    await feed.insert({ data: { site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 1));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
});

add_task(async function test_insert_part_2() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info(
      "TopSitesFeed.insert should unpin the last site if all slots are " +
        "already pinned"
    );
    let site1 = { url: "example.com" };
    let site2 = { url: "example.org" };
    let site3 = { url: "example.net" };
    let site4 = { url: "example.biz" };
    let site5 = { url: "example.info" };
    let site6 = { url: "example.news" };
    let site7 = { url: "example.lol" };
    let site8 = { url: "example.golf" };
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [site1, site2, site3, site4, site5, site6, site7, site8]);
    let feed = getTopSitesFeedForTest(sandbox);
    feed.store.state.Prefs.values.topSitesRows = 1;
    let site = { url: "foo.bar", label: "foo" };
    await feed.insert({ data: { site } });
    Assert.equal(
      NewTabUtils.pinnedLinks.pin.callCount,
      8,
      "NewTabUtils.pinnedLinks.pin called 8 times"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 1));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site2, 2));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site3, 3));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site4, 4));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site5, 5));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site6, 6));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site7, 7));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info("TopSitesFeed.insert should trigger refresh on TOP_SITES_INSERT");
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.stub(feed, "refresh");
    let addAction = {
      type: at.TOP_SITES_INSERT,
      data: { site: { url: "foo.com" } },
    };

    await feed.insert(addAction);

    Assert.ok(feed.refresh.calledOnce, "feed.refresh called once");
  }

  {
    info("TopSitesFeed.insert should correctly handle different index values");
    let index = -1;
    let site = { url: "foo.bar", label: "foo" };
    let action = { data: { index, site } };
    let feed = getTopSitesFeedForTest(sandbox);

    await feed.insert(action);
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));

    index = undefined;
    await feed.insert(action);
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
});

add_task(async function test_insert_part_3() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info("TopSitesFeed.insert should pin site in specified slot that is free");
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };
    let feed = getTopSitesFeedForTest(sandbox);

    await feed.insert({ data: { index: 2, site, draggedFromIndex: 0 } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.insert should move a pinned site in specified slot " +
        "to the next slot"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };
    let feed = getTopSitesFeedForTest(sandbox);

    await feed.insert({ data: { index: 2, site, draggedFromIndex: 3 } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledWith({ url: "example.com" }, 3)
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.insert should move pinned sites in the direction " +
        "of the dragged site"
    );

    let site1 = { url: "foo.bar", label: "foo" };
    let site2 = { url: "example.com", label: "example" };
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, null, site2]);

    let feed = getTopSitesFeedForTest(sandbox);

    await feed.insert({ data: { index: 2, site: site1, draggedFromIndex: 0 } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 2));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site2, 1));
    NewTabUtils.pinnedLinks.pin.resetHistory();

    await feed.insert({ data: { index: 2, site: site1, draggedFromIndex: 5 } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 2));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site2, 3));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info("TopSitesFeed.insert should not insert past the visible top sites");

    let feed = getTopSitesFeedForTest(sandbox);
    let site1 = { url: "foo.bar", label: "foo" };
    await feed.insert({
      data: { index: 42, site: site1, draggedFromIndex: 0 },
    });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.notCalled,
      "NewTabUtils.pinnedLinks.pin wasn't called"
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
});

add_task(async function test_pin_part_1() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info(
      "TopSitesFeed.pin should pin site in specified slot empty pinned " +
        "list"
    );
    let site = {
      url: "foo.bar",
      label: "foo",
      customScreenshotURL: "screenshot",
    };
    let feed = getTopSitesFeedForTest(sandbox);
    await feed.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.pin should lookup the link object to update the custom " +
        "screenshot"
    );
    let site = {
      url: "foo.bar",
      label: "foo",
      customScreenshotURL: "screenshot",
    };
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.spy(feed.pinnedCache, "request");
    await feed.pin({ data: { index: 2, site } });

    Assert.ok(
      feed.pinnedCache.request.calledOnce,
      "feed.pinnedCache.request called once"
    );
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.pin should lookup the link object to update the custom " +
        "screenshot when the custom screenshot is initially null"
    );
    let site = {
      url: "foo.bar",
      label: "foo",
      customScreenshotURL: null,
    };
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.spy(feed.pinnedCache, "request");
    await feed.pin({ data: { index: 2, site } });

    Assert.ok(
      feed.pinnedCache.request.calledOnce,
      "feed.pinnedCache.request called once"
    );
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.pin should not do a link object lookup if custom " +
        "screenshot field is not set"
    );
    let site = { url: "foo.bar", label: "foo" };
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.spy(feed.pinnedCache, "request");
    await feed.pin({ data: { index: 2, site } });

    Assert.ok(
      !feed.pinnedCache.request.called,
      "feed.pinnedCache.request never called"
    );
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.pin should pin site in specified slot of pinned " +
        "list that is free"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };
    let feed = getTopSitesFeedForTest(sandbox);
    await feed.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
});

add_task(async function test_pin_part_2() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info("TopSitesFeed.pin should save the searchTopSite attribute if set");
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo", searchTopSite: true };
    let feed = getTopSitesFeedForTest(sandbox);
    await feed.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.firstCall.args[0].searchTopSite);
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.pin should NOT move a pinned site in specified " +
        "slot to the next slot"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };
    let feed = getTopSitesFeedForTest(sandbox);
    await feed.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.pin should properly update LinksCache object " +
        "properties between migrations"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "https://foo.com/" }]);

    let feed = getTopSitesFeedForTest(sandbox);
    let pinnedLinks = await feed.pinnedCache.request();
    Assert.equal(pinnedLinks.length, 1);
    feed.pinnedCache.expire();

    pinnedLinks[0].__sharedCache.updateLink("screenshot", "foo");

    pinnedLinks = await feed.pinnedCache.request();
    Assert.equal(pinnedLinks[0].screenshot, "foo");

    // Force cache expiration in order to trigger a migration of objects
    feed.pinnedCache.expire();
    pinnedLinks[0].__sharedCache.updateLink("screenshot", "bar");

    pinnedLinks = await feed.pinnedCache.request();
    Assert.equal(pinnedLinks[0].screenshot, "bar");
  }

  sandbox.restore();
});

add_task(async function test_pin_part_3() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info("TopSitesFeed.pin should call insert if index < 0");
    let site = { url: "foo.bar", label: "foo" };
    let action = { data: { index: -1, site } };
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.spy(feed, "insert");
    await feed.pin(action);

    Assert.ok(feed.insert.calledOnce, "feed.insert called once");
    Assert.ok(feed.insert.calledWithExactly(action));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info("TopSitesFeed.pin should not call insert if index == 0");
    let site = { url: "foo.bar", label: "foo" };
    let action = { data: { index: 0, site } };
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.spy(feed, "insert");
    await feed.pin(action);

    Assert.ok(!feed.insert.called, "feed.insert not called");
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info("TopSitesFeed.pin should trigger refresh on TOP_SITES_PIN");
    let feed = getTopSitesFeedForTest(sandbox);
    sandbox.stub(feed, "refresh");
    let pinExistingAction = {
      type: at.TOP_SITES_PIN,
      data: { site: FAKE_LINKS[4], index: 4 },
    };

    await feed.pin(pinExistingAction);

    Assert.ok(feed.refresh.calledOnce, "feed.refresh called once");
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
});

add_task(async function test_integration() {
  let sandbox = sinon.createSandbox();

  info("Test adding a pinned site and removing it with actions");
  let feed = getTopSitesFeedForTest(sandbox);

  let resolvers = [];
  feed.store.dispatch = sandbox.stub().callsFake(() => {
    resolvers.shift()();
  });
  feed._startedUp = true;
  sandbox.stub(feed, "_fetchScreenshot");

  let forDispatch = action =>
    new Promise(resolve => {
      resolvers.push(resolve);
      feed.onAction(action);
    });

  feed._requestRichIcon = sandbox.stub();
  let url = "https://pin.me";
  sandbox.stub(NewTabUtils.pinnedLinks, "pin").callsFake(link => {
    NewTabUtils.pinnedLinks.links.push(link);
  });

  await forDispatch({ type: at.TOP_SITES_INSERT, data: { site: { url } } });
  NewTabUtils.pinnedLinks.links.pop();
  await forDispatch({ type: at.PLACES_LINK_BLOCKED });

  Assert.ok(
    feed.store.dispatch.calledTwice,
    "feed.store.dispatch called twice"
  );
  Assert.equal(feed.store.dispatch.firstCall.args[0].data.links[0].url, url);
  Assert.equal(
    feed.store.dispatch.secondCall.args[0].data.links[0].url,
    FAKE_LINKS[0].url
  );

  sandbox.restore();
});

add_task(async function test_improvesearch_noDefaultSearchTile_experiment() {
  let sandbox = sinon.createSandbox();
  const NO_DEFAULT_SEARCH_TILE_PREF = "improvesearch.noDefaultSearchTile";

  sandbox.stub(SearchService.prototype, "getDefault").resolves({
    identifier: "google",
    searchForm: "google.com",
  });

  {
    info(
      "TopSitesFeed.getLinksWithDefaults should filter out alexa top 5 " +
        "search from the default sites"
    );
    let feed = getTopSitesFeedForTest(sandbox);
    feed.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;
    let top5Test = [
      "https://google.com",
      "https://search.yahoo.com",
      "https://yahoo.com",
      "https://bing.com",
      "https://ask.com",
      "https://duckduckgo.com",
    ];

    gGetTopSitesStub.resolves([
      { url: "https://amazon.com" },
      ...top5Test.map(url => ({ url })),
    ]);

    const urlsReturned = (await feed.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(
      urlsReturned.includes("https://amazon.com"),
      "amazon included in default links"
    );
    top5Test.forEach(url =>
      Assert.ok(!urlsReturned.includes(url), `Should not include ${url}`)
    );

    gGetTopSitesStub.resolves(FAKE_LINKS);
  }

  {
    info(
      "TopSitesFeed.getLinksWithDefaults should not filter out alexa, default " +
        "search from the query results if the experiment pref is off"
    );
    let feed = getTopSitesFeedForTest(sandbox);
    feed.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = false;

    gGetTopSitesStub.resolves([
      { url: "https://google.com" },
      { url: "https://foo.com" },
      { url: "https://duckduckgo" },
    ]);
    let urlsReturned = (await feed.getLinksWithDefaults()).map(
      link => link.url
    );

    Assert.ok(urlsReturned.includes("https://google.com"));
    gGetTopSitesStub.resolves(FAKE_LINKS);
  }

  {
    info(
      "TopSitesFeed.getLinksWithDefaults should filter out the current " +
        "default search from the default sites"
    );
    let feed = getTopSitesFeedForTest(sandbox);
    feed.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;

    sandbox.stub(feed, "_currentSearchHostname").get(() => "amazon");
    feed.onAction({
      type: at.PREFS_INITIAL_VALUES,
      data: { "default.sites": "google.com,amazon.com" },
    });
    gGetTopSitesStub.resolves([{ url: "https://foo.com" }]);

    let urlsReturned = (await feed.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(!urlsReturned.includes("https://amazon.com"));

    gGetTopSitesStub.resolves(FAKE_LINKS);
  }

  {
    info(
      "TopSitesFeed.getLinksWithDefaults should not filter out current " +
        "default search from pinned sites even if it matches the current " +
        "default search"
    );
    let feed = getTopSitesFeedForTest(sandbox);
    feed.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;

    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "google.com" }]);
    gGetTopSitesStub.resolves([{ url: "https://foo.com" }]);

    let urlsReturned = (await feed.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(urlsReturned.includes("google.com"));

    gGetTopSitesStub.resolves(FAKE_LINKS);
  }

  sandbox.restore();
});

add_task(
  async function test_improvesearch_noDefaultSearchTile_experiment_part_2() {
    let sandbox = sinon.createSandbox();
    const NO_DEFAULT_SEARCH_TILE_PREF = "improvesearch.noDefaultSearchTile";

    sandbox.stub(SearchService.prototype, "getDefault").resolves({
      identifier: "google",
      searchForm: "google.com",
    });

    {
      info(
        "TopSitesFeed.getLinksWithDefaults should call refresh and set " +
          "._currentSearchHostname to the new engine hostname when the " +
          "default search engine has been set"
      );
      let feed = getTopSitesFeedForTest(sandbox);
      feed.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;
      sandbox.stub(feed, "refresh");

      feed.observe(null, "browser-search-engine-modified", "engine-default");
      Assert.equal(feed._currentSearchHostname, "duckduckgo");
      Assert.ok(feed.refresh.calledOnce, "feed.refresh called once");

      gGetTopSitesStub.resolves(FAKE_LINKS);
    }

    {
      info(
        "TopSitesFeed.getLinksWithDefaults should call refresh when the " +
          "experiment pref has changed"
      );
      let feed = getTopSitesFeedForTest(sandbox);
      feed.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;
      sandbox.stub(feed, "refresh");

      feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: NO_DEFAULT_SEARCH_TILE_PREF, value: true },
      });
      Assert.ok(feed.refresh.calledOnce, "feed.refresh was called once");

      feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: NO_DEFAULT_SEARCH_TILE_PREF, value: false },
      });
      Assert.ok(feed.refresh.calledTwice, "feed.refresh was called twice");

      gGetTopSitesStub.resolves(FAKE_LINKS);
    }

    sandbox.restore();
  }
);

// eslint-disable-next-line max-statements
add_task(async function test_improvesearch_topSitesSearchShortcuts() {
  let sandbox = sinon.createSandbox();
  let searchEngines = [{ aliases: ["@google"] }, { aliases: ["@amazon"] }];
  sandbox
    .stub(SearchService.prototype, "getAppProvidedEngines")
    .resolves(searchEngines);
  sandbox.stub(NewTabUtils.pinnedLinks, "pin").callsFake((site, index) => {
    NewTabUtils.pinnedLinks.links[index] = site;
  });

  let prepFeed = feed => {
    feed.store.state.Prefs.values[SEARCH_SHORTCUTS_EXPERIMENT_PREF] = true;
    feed.store.state.Prefs.values[SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF] =
      "google,amazon";
    feed.store.state.Prefs.values[SEARCH_SHORTCUTS_HAVE_PINNED_PREF] = "";
    return feed;
  };

  {
    info(
      "TopSitesFeed should updateCustomSearchShortcuts when experiment " +
        "pref is turned on"
    );
    let feed = prepFeed(getTopSitesFeedForTest(sandbox));
    feed.store.state.Prefs.values[SEARCH_SHORTCUTS_EXPERIMENT_PREF] = false;
    feed.updateCustomSearchShortcuts = sandbox.spy();

    // turn the experiment on
    feed.onAction({
      type: at.PREF_CHANGED,
      data: { name: SEARCH_SHORTCUTS_EXPERIMENT_PREF, value: true },
    });

    Assert.ok(
      feed.updateCustomSearchShortcuts.calledOnce,
      "feed.updateCustomSearchShortcuts called once"
    );
  }

  {
    info(
      "TopSitesFeed should filter out default top sites that match a " +
        "hostname of a search shortcut if previously blocked"
    );
    let feed = prepFeed(getTopSitesFeedForTest(sandbox));
    feed.refreshDefaults("https://amazon.ca");
    sandbox
      .stub(NewTabUtils.blockedLinks, "links")
      .value([{ url: "https://amazon.com" }]);
    sandbox.stub(NewTabUtils.blockedLinks, "isBlocked").callsFake(site => {
      return NewTabUtils.blockedLinks.links[0].url === site.url;
    });

    let urlsReturned = (await feed.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(!urlsReturned.includes("https://amazon.ca"));
  }

  {
    info("TopSitesFeed should update frecent search topsite icon");
    let feed = prepFeed(getTopSitesFeedForTest(sandbox));
    feed._tippyTopProvider.processSite = site => {
      site.tippyTopIcon = "icon.png";
      site.backgroundColor = "#fff";
      return site;
    };
    gGetTopSitesStub.resolves([{ url: "https://google.com" }]);

    let urlsReturned = await feed.getLinksWithDefaults();

    let defaultSearchTopsite = urlsReturned.find(
      s => s.url === "https://google.com"
    );
    Assert.ok(defaultSearchTopsite.searchTopSite);
    Assert.equal(defaultSearchTopsite.tippyTopIcon, "icon.png");
    Assert.equal(defaultSearchTopsite.backgroundColor, "#fff");
    gGetTopSitesStub.resolves(FAKE_LINKS);
  }

  {
    info("TopSitesFeed should update default search topsite icon");
    let feed = prepFeed(getTopSitesFeedForTest(sandbox));
    feed._tippyTopProvider.processSite = site => {
      site.tippyTopIcon = "icon.png";
      site.backgroundColor = "#fff";
      return site;
    };
    gGetTopSitesStub.resolves([{ url: "https://foo.com" }]);
    feed.onAction({
      type: at.PREFS_INITIAL_VALUES,
      data: { "default.sites": "google.com,amazon.com" },
    });

    let urlsReturned = await feed.getLinksWithDefaults();

    let defaultSearchTopsite = urlsReturned.find(
      s => s.url === "https://amazon.com"
    );
    Assert.ok(defaultSearchTopsite.searchTopSite);
    Assert.equal(defaultSearchTopsite.tippyTopIcon, "icon.png");
    Assert.equal(defaultSearchTopsite.backgroundColor, "#fff");
    gGetTopSitesStub.resolves(FAKE_LINKS);
  }

  {
    info(
      "TopSitesFeed should dispatch UPDATE_SEARCH_SHORTCUTS on " +
        "updateCustomSearchShortcuts"
    );
    let feed = prepFeed(getTopSitesFeedForTest(sandbox));
    feed.store.state.Prefs.values["improvesearch.noDefaultSearchTile"] = true;
    await feed.updateCustomSearchShortcuts();
    Assert.ok(
      feed.store.dispatch.calledOnce,
      "feed.store.dispatch called once"
    );
    Assert.ok(
      feed.store.dispatch.calledWith({
        data: {
          searchShortcuts: [
            {
              keyword: "@google",
              shortURL: "google",
              url: "https://google.com",
              backgroundColor: undefined,
              smallFavicon:
                "chrome://activity-stream/content/data/content/tippytop/favicons/google-com.ico",
              tippyTopIcon:
                "chrome://activity-stream/content/data/content/tippytop/images/google-com@2x.png",
            },
            {
              keyword: "@amazon",
              shortURL: "amazon",
              url: "https://amazon.com",
              backgroundColor: undefined,
              smallFavicon:
                "chrome://activity-stream/content/data/content/tippytop/favicons/amazon.ico",
              tippyTopIcon:
                "chrome://activity-stream/content/data/content/tippytop/images/amazon@2x.png",
            },
          ],
        },
        meta: {
          from: "ActivityStream:Main",
          to: "ActivityStream:Content",
          isStartup: false,
        },
        type: "UPDATE_SEARCH_SHORTCUTS",
      })
    );
  }

  sandbox.restore();
});

add_task(async function test_updatePinnedSearchShortcuts() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  sandbox.stub(NewTabUtils.pinnedLinks, "unpin");

  {
    info(
      "TopSitesFeed.updatePinnedSearchShortcuts should unpin a " +
        "shortcut in deletedShortcuts"
    );
    let feed = getTopSitesFeedForTest(sandbox);

    let deletedShortcuts = [
      {
        url: "https://google.com",
        searchVendor: "google",
        label: "google",
        searchTopSite: true,
      },
    ];
    let addedShortcuts = [];
    sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [
      null,
      null,
      {
        url: "https://amazon.com",
        searchVendor: "amazon",
        label: "amazon",
        searchTopSite: true,
      },
    ]);

    feed.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.notCalled,
      "NewTabUtils.pinnedLinks.pin not called"
    );
    Assert.ok(
      NewTabUtils.pinnedLinks.unpin.calledOnce,
      "NewTabUtils.pinnedLinks.unpin called once"
    );
    Assert.ok(
      NewTabUtils.pinnedLinks.unpin.calledWith({
        url: "https://google.com",
      })
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
    NewTabUtils.pinnedLinks.unpin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.updatePinnedSearchShortcuts should pin a shortcut " +
        "in addedShortcuts"
    );
    let feed = getTopSitesFeedForTest(sandbox);

    let addedShortcuts = [
      {
        url: "https://google.com",
        searchVendor: "google",
        label: "google",
        searchTopSite: true,
      },
    ];
    let deletedShortcuts = [];
    sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [
      null,
      null,
      {
        url: "https://amazon.com",
        searchVendor: "amazon",
        label: "amazon",
        searchTopSite: true,
      },
    ]);
    feed.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });

    Assert.ok(
      NewTabUtils.pinnedLinks.unpin.notCalled,
      "NewTabUtils.pinnedLinks.unpin not called"
    );
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledWith(
        {
          label: "google",
          searchTopSite: true,
          searchVendor: "google",
          url: "https://google.com",
        },
        0
      )
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
    NewTabUtils.pinnedLinks.unpin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.updatePinnedSearchShortcuts should pin and unpin " +
        "in the same action"
    );
    let feed = getTopSitesFeedForTest(sandbox);

    let addedShortcuts = [
      {
        url: "https://google.com",
        searchVendor: "google",
        label: "google",
        searchTopSite: true,
      },
      {
        url: "https://ebay.com",
        searchVendor: "ebay",
        label: "ebay",
        searchTopSite: true,
      },
    ];
    let deletedShortcuts = [
      {
        url: "https://amazon.com",
        searchVendor: "amazon",
        label: "amazon",
        searchTopSite: true,
      },
    ];

    sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [
      { url: "https://foo.com" },
      {
        url: "https://amazon.com",
        searchVendor: "amazon",
        label: "amazon",
        searchTopSite: true,
      },
    ]);

    feed.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });

    Assert.ok(
      NewTabUtils.pinnedLinks.unpin.calledOnce,
      "NewTabUtils.pinnedLinks.unpin called once"
    );
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
    NewTabUtils.pinnedLinks.unpin.resetHistory();
  }

  {
    info(
      "TopSitesFeed.updatePinnedSearchShortcuts should pin a shortcut in " +
        "addedShortcuts even if pinnedLinks is full"
    );
    let feed = getTopSitesFeedForTest(sandbox);

    let addedShortcuts = [
      {
        url: "https://google.com",
        searchVendor: "google",
        label: "google",
        searchTopSite: true,
      },
    ];
    let deletedShortcuts = [];
    sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => FAKE_LINKS);
    feed.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });

    Assert.ok(
      NewTabUtils.pinnedLinks.unpin.notCalled,
      "NewTabUtils.pinnedLinks.unpin not called"
    );
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledWith(
        { label: "google", searchTopSite: true, url: "https://google.com" },
        0
      ),
      "NewTabUtils.pinnedLinks.unpin not called"
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
    NewTabUtils.pinnedLinks.unpin.resetHistory();
  }

  sandbox.restore();
});

// eslint-disable-next-line max-statements
add_task(async function test_ContileIntegration() {
  let sandbox = sinon.createSandbox();
  Services.prefs.setStringPref(
    TOP_SITES_BLOCKED_SPONSORS_PREF,
    `["foo","bar"]`
  );
  sandbox.stub(NimbusFeatures.newtab, "getVariable").returns(true);

  let prepFeed = feed => {
    feed.store.state.Prefs.values[SHOW_SPONSORED_PREF] = true;
    let fetchStub = sandbox.stub(feed, "fetch");
    return { feed, fetchStub };
  };

  {
    info("TopSitesFeed._fetchSites should fetch sites from Contile");
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://www.test.com",
              image_url: "images/test-com.png",
              click_url: "https://www.test-click.com",
              impression_url: "https://www.test-impression.com",
              name: "test",
            },
            {
              url: "https://www.test1.com",
              image_url: "images/test1-com.png",
              click_url: "https://www.test1-click.com",
              impression_url: "https://www.test1-impression.com",
              name: "test1",
            },
          ],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 2);
  }

  {
    info("TopSitesFeed._fetchSites should call allocatePositions");
    let { feed } = prepFeed(getTopSitesFeedForTest(sandbox));
    sandbox.stub(feed, "allocatePositions").resolves();
    await feed._contile.refresh();

    Assert.ok(
      feed.allocatePositions.calledOnce,
      "feed.allocatePositions called once"
    );
  }

  {
    info(
      "TopSitesFeed._fetchSites should fetch SOV (Share-of-Voice) " +
        "settings from Contile"
    );
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    let sov = {
      name: "SOV-20230518215316",
      allocations: [
        {
          position: 1,
          allocation: [
            {
              partner: "foo",
              percentage: 100,
            },
            {
              partner: "bar",
              percentage: 0,
            },
          ],
        },
        {
          position: 2,
          allocation: [
            {
              partner: "foo",
              percentage: 80,
            },
            {
              partner: "bar",
              percentage: 20,
            },
          ],
        },
      ],
    };
    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          sov: btoa(JSON.stringify(sov)),
          tiles: [
            {
              url: "https://www.test.com",
              image_url: "images/test-com.png",
              click_url: "https://www.test-click.com",
              impression_url: "https://www.test-impression.com",
              name: "test",
            },
            {
              url: "https://www.test1.com",
              image_url: "images/test1-com.png",
              click_url: "https://www.test1-click.com",
              impression_url: "https://www.test1-impression.com",
              name: "test1",
            },
          ],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    Assert.deepEqual(feed._contile.sov, sov);
    Assert.equal(feed._contile.sites.length, 2);
  }

  {
    info(
      "TopSitesFeed._fetchSites should not fetch from Contile if " +
        "it's not enabled"
    );
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    NimbusFeatures.newtab.getVariable.reset();
    NimbusFeatures.newtab.getVariable.returns(false);
    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetchStub.notCalled, "TopSitesFeed.fetch was not called");
    Assert.ok(!fetched);
    Assert.equal(feed._contile.sites.length, 0);
  }

  {
    info(
      "TopSitesFeed._fetchSites should still return two tiles when Contile " +
        "provides more than 2 tiles and filtering results in more than 2 tiles"
    );
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    NimbusFeatures.newtab.getVariable.reset();
    NimbusFeatures.newtab.getVariable.onCall(0).returns(true);
    NimbusFeatures.newtab.getVariable.onCall(1).returns(true);

    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://www.test.com",
              image_url: "images/test-com.png",
              click_url: "https://www.test-click.com",
              impression_url: "https://www.test-impression.com",
              name: "test",
            },
            {
              url: "https://foo.com",
              image_url: "images/foo-com.png",
              click_url: "https://www.foo-click.com",
              impression_url: "https://www.foo-impression.com",
              name: "foo",
            },
            {
              url: "https://bar.com",
              image_url: "images/bar-com.png",
              click_url: "https://www.bar-click.com",
              impression_url: "https://www.bar-impression.com",
              name: "bar",
            },
            {
              url: "https://test1.com",
              image_url: "images/test1-com.png",
              click_url: "https://www.test1-click.com",
              impression_url: "https://www.test1-impression.com",
              name: "test1",
            },
            {
              url: "https://test2.com",
              image_url: "images/test2-com.png",
              click_url: "https://www.test2-click.com",
              impression_url: "https://www.test2-impression.com",
              name: "test2",
            },
          ],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    // Both "foo" and "bar" should be filtered
    Assert.equal(feed._contile.sites.length, 2);
    Assert.equal(feed._contile.sites[0].url, "https://www.test.com");
    Assert.equal(feed._contile.sites[1].url, "https://test1.com");
  }

  {
    info(
      "TopSitesFeed._fetchSites should still return two tiles with " +
        "replacement if the Nimbus variable was unset"
    );
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    NimbusFeatures.newtab.getVariable.reset();
    NimbusFeatures.newtab.getVariable.onCall(0).returns(true);
    NimbusFeatures.newtab.getVariable.onCall(1).returns(undefined);

    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://www.test.com",
              image_url: "images/test-com.png",
              click_url: "https://www.test-click.com",
              impression_url: "https://www.test-impression.com",
              name: "test",
            },
            {
              url: "https://foo.com",
              image_url: "images/foo-com.png",
              click_url: "https://www.foo-click.com",
              impression_url: "https://www.foo-impression.com",
              name: "foo",
            },
            {
              url: "https://test1.com",
              image_url: "images/test1-com.png",
              click_url: "https://www.test1-click.com",
              impression_url: "https://www.test1-impression.com",
              name: "test1",
            },
          ],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 2);
    Assert.equal(feed._contile.sites[0].url, "https://www.test.com");
    Assert.equal(feed._contile.sites[1].url, "https://test1.com");
  }

  {
    info("TopSitesFeed._fetchSites should filter the blocked sponsors");
    NimbusFeatures.newtab.getVariable.returns(true);

    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://www.test.com",
              image_url: "images/test-com.png",
              click_url: "https://www.test-click.com",
              impression_url: "https://www.test-impression.com",
              name: "test",
            },
            {
              url: "https://foo.com",
              image_url: "images/foo-com.png",
              click_url: "https://www.foo-click.com",
              impression_url: "https://www.foo-impression.com",
              name: "foo",
            },
            {
              url: "https://bar.com",
              image_url: "images/bar-com.png",
              click_url: "https://www.bar-click.com",
              impression_url: "https://www.bar-impression.com",
              name: "bar",
            },
          ],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    // Both "foo" and "bar" should be filtered
    Assert.equal(feed._contile.sites.length, 1);
    Assert.equal(feed._contile.sites[0].url, "https://www.test.com");
  }

  {
    info(
      "TopSitesFeed._fetchSites should return false when Contile returns " +
        "with error status and no values are stored in cache prefs"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    Services.prefs.setStringPref(CONTILE_CACHE_PREF, "[]");
    Services.prefs.setIntPref(CONTILE_CACHE_LAST_FETCH_PREF, 0);

    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    fetchStub.resolves({
      ok: false,
      status: 500,
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(!fetched);
    Assert.ok(!feed._contile.sites.length);
  }

  {
    info(
      "TopSitesFeed._fetchSites should return false when Contile " +
        "returns with error status and cached tiles are expried"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    Services.prefs.setStringPref(CONTILE_CACHE_PREF, "[]");
    const THIRTY_MINUTES_AGO_IN_SECONDS =
      Math.round(Date.now() / 1000) - 60 * 30;
    Services.prefs.setIntPref(
      CONTILE_CACHE_LAST_FETCH_PREF,
      THIRTY_MINUTES_AGO_IN_SECONDS
    );
    Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_SECONDS_PREF, 60 * 15);

    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    fetchStub.resolves({
      ok: false,
      status: 500,
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(!fetched);
    Assert.ok(!feed._contile.sites.length);
  }

  {
    info(
      "TopSitesFeed._fetchSites should handle invalid payload " +
        "properly from Contile"
    );
    NimbusFeatures.newtab.getVariable.returns(true);

    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    fetchStub.resolves({
      ok: true,
      status: 200,
      json: () =>
        Promise.resolve({
          unknown: [],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(!fetched);
    Assert.ok(!feed._contile.sites.length);
  }

  {
    info(
      "TopSitesFeed._fetchSites should handle empty payload properly " +
        "from Contile"
    );
    NimbusFeatures.newtab.getVariable.returns(true);

    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    Assert.ok(!feed._contile.sites.length);
  }

  {
    info(
      "TopSitesFeed._fetchSites should handle no content properly " +
        "from Contile"
    );
    NimbusFeatures.newtab.getVariable.returns(true);

    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    fetchStub.resolves({ ok: true, status: 204 });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(!fetched);
    Assert.ok(!feed._contile.sites.length);
  }

  {
    info(
      "TopSitesFeed._fetchSites should set Caching Prefs after " +
        "a successful request"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    let tiles = [
      {
        url: "https://www.test.com",
        image_url: "images/test-com.png",
        click_url: "https://www.test-click.com",
        impression_url: "https://www.test-impression.com",
        name: "test",
      },
      {
        url: "https://www.test1.com",
        image_url: "images/test1-com.png",
        click_url: "https://www.test1-click.com",
        impression_url: "https://www.test1-impression.com",
        name: "test1",
      },
    ];
    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles,
        }),
    });

    let fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(
      Services.prefs.getStringPref(CONTILE_CACHE_PREF),
      JSON.stringify(tiles)
    );
    Assert.equal(
      Services.prefs.getIntPref(CONTILE_CACHE_VALID_FOR_SECONDS_PREF),
      11322
    );
  }

  {
    info(
      "TopSitesFeed._fetchSites should return cached valid tiles " +
        "when Contile returns error status"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    let tiles = [
      {
        url: "https://www.test-cached.com",
        image_url: "images/test-com.png",
        click_url: "https://www.test-click.com",
        impression_url: "https://www.test-impression.com",
        name: "test",
      },
      {
        url: "https://www.test1-cached.com",
        image_url: "images/test1-com.png",
        click_url: "https://www.test1-click.com",
        impression_url: "https://www.test1-impression.com",
        name: "test1",
      },
    ];

    Services.prefs.setStringPref(CONTILE_CACHE_PREF, JSON.stringify(tiles));
    Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_SECONDS_PREF, 60 * 15);
    Services.prefs.setIntPref(
      CONTILE_CACHE_LAST_FETCH_PREF,
      Math.round(Date.now() / 1000)
    );

    fetchStub.resolves({
      status: 304,
    });

    let fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 2);
    Assert.equal(feed._contile.sites[0].url, "https://www.test-cached.com");
    Assert.equal(feed._contile.sites[1].url, "https://www.test1-cached.com");
  }

  {
    info(
      "TopSitesFeed._fetchSites should not be successful when contile " +
        "returns an error and no valid tiles are cached"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));
    Services.prefs.setStringPref(CONTILE_CACHE_PREF, "[]");
    Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_SECONDS_PREF, 0);
    Services.prefs.setIntPref(CONTILE_CACHE_LAST_FETCH_PREF, 0);

    fetchStub.resolves({
      status: 500,
    });

    let fetched = await feed._contile._fetchSites();
    Assert.ok(!fetched);
  }

  {
    info(
      "TopSitesFeed._fetchSites should return cached valid tiles " +
        "filtering blocked tiles when Contile returns error status"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    let tiles = [
      {
        url: "https://foo.com",
        image_url: "images/foo-com.png",
        click_url: "https://www.foo-click.com",
        impression_url: "https://www.foo-impression.com",
        name: "foo",
      },
      {
        url: "https://www.test1-cached.com",
        image_url: "images/test1-com.png",
        click_url: "https://www.test1-click.com",
        impression_url: "https://www.test1-impression.com",
        name: "test1",
      },
    ];
    Services.prefs.setStringPref(CONTILE_CACHE_PREF, JSON.stringify(tiles));
    Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_SECONDS_PREF, 60 * 15);
    Services.prefs.setIntPref(
      CONTILE_CACHE_LAST_FETCH_PREF,
      Math.round(Date.now() / 1000)
    );

    fetchStub.resolves({
      status: 304,
    });

    let fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 1);
    Assert.equal(feed._contile.sites[0].url, "https://www.test1-cached.com");
  }

  {
    info(
      "TopSitesFeed._fetchSites should still return 3 tiles when nimbus " +
        "variable overrides max num of sponsored contile tiles"
    );
    NimbusFeatures.newtab.getVariable.returns(true);
    let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox));

    sandbox.stub(NimbusFeatures.pocketNewtab, "getVariable").returns(3);
    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://www.test.com",
              image_url: "images/test-com.png",
              click_url: "https://www.test-click.com",
              impression_url: "https://www.test-impression.com",
              name: "test",
            },
            {
              url: "https://test1.com",
              image_url: "images/test1-com.png",
              click_url: "https://www.test1-click.com",
              impression_url: "https://www.test1-impression.com",
              name: "test1",
            },
            {
              url: "https://test2.com",
              image_url: "images/test2-com.png",
              click_url: "https://www.test2-click.com",
              impression_url: "https://www.test2-impression.com",
              name: "test2",
            },
          ],
        }),
    });

    let fetched = await feed._contile._fetchSites();

    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 3);
    Assert.equal(feed._contile.sites[0].url, "https://www.test.com");
    Assert.equal(feed._contile.sites[1].url, "https://test1.com");
    Assert.equal(feed._contile.sites[2].url, "https://test2.com");
  }

  Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
  sandbox.restore();
});
