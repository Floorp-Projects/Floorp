/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TopSites, DEFAULT_TOP_SITES } = ChromeUtils.importESModule(
  "resource:///modules/TopSites.sys.mjs"
);

const { actionCreators: ac, actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  FilterAdult: "resource://activity-stream/lib/FilterAdult.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PageThumbs: "resource://gre/modules/PageThumbs.sys.mjs",
  shortURL: "resource://activity-stream/lib/ShortURL.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  Screenshots: "resource://activity-stream/lib/Screenshots.sys.mjs",
  SearchService: "resource://gre/modules/SearchService.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
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

function stubTopSites(sandbox) {
  let cachedStorage = TopSites._storage;
  let cachedStore = TopSites.store;

  async function cleanup() {
    if (TopSites._refreshing) {
      info("Wait for refresh to finish.");
      // Wait for refresh to finish or else removing the store while a process
      // is running will result in errors.
      await TestUtils.topicObserved("topsites-refreshed");
    }
    TopSites._tippyTopProvider.initialized = false;
    TopSites._storage = cachedStorage;
    TopSites.store = cachedStore;
    TopSites.pinnedCache.clear();
    TopSites.frecentCache.clear();
    info("Finished cleaning up TopSites.");
  }

  const storage = {
    init: sandbox.stub().resolves(),
    get: sandbox.stub().resolves(),
    set: sandbox.stub().resolves(),
  };

  // Setup for tests that don't call `init` but require feed.storage
  TopSites._storage = storage;
  TopSites.store = {
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
  info("Created mock store for TopSites.");
  return cleanup;
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
  Assert.ok(TopSites._currentSearchHostname, "_currentSearchHostname defined");
});

add_task(async function test_refreshDefaults() {
  let sandbox = sinon.createSandbox();
  let cleanup = stubTopSites(sandbox);
  Assert.ok(
    !DEFAULT_TOP_SITES.length,
    "Should have 0 DEFAULT_TOP_SITES initially."
  );

  info("refreshDefaults should add defaults on PREFS_INITIAL_VALUES");
  TopSites.onAction({
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
  TopSites.onAction({
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
  let refreshStub = sandbox.stub(TopSites, "refresh");
  TopSites.onAction({ type: at.PREF_CHANGED, data: { name: "topSitesRows" } });
  Assert.ok(TopSites.refresh.calledOnce, "refresh called");
  refreshStub.restore();

  // Reset the DEFAULT_TOP_SITES;
  DEFAULT_TOP_SITES.length = 0;

  info("refreshDefaults should have default sites with .isDefault = true");
  TopSites.refreshDefaults("https://foo.com");
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
  TopSites.refreshDefaults("https://foo.com");
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
  TopSites.refreshDefaults("");
  Assert.equal(
    DEFAULT_TOP_SITES.length,
    0,
    "Should have 0 DEFAULT_TOP_SITES now."
  );

  info("refreshDefaults should be able to clear defaults");
  TopSites.refreshDefaults("https://foo.com");
  TopSites.refreshDefaults("");

  Assert.equal(
    DEFAULT_TOP_SITES.length,
    0,
    "Should have 0 DEFAULT_TOP_SITES now."
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_filterForThumbnailExpiration() {
  let sandbox = sinon.createSandbox();
  let cleanup = stubTopSites(sandbox);

  info(
    "filterForThumbnailExpiration should pass rows.urls to the callback provided"
  );
  const rows = [
    { url: "foo.com" },
    { url: "bar.com", customScreenshotURL: "custom" },
  ];
  TopSites.store.state.TopSites = { rows };
  const stub = sandbox.stub();
  TopSites.filterForThumbnailExpiration(stub);
  Assert.ok(stub.calledOnce);
  Assert.ok(stub.calledWithExactly(["foo.com", "bar.com", "custom"]));

  sandbox.restore();
  await cleanup();
});

add_task(
  async function test_getLinksWithDefaults_on_SearchService_init_failure() {
    let sandbox = sinon.createSandbox();
    let cleanup = stubTopSites(sandbox);

    TopSites.refreshDefaults("https://foo.com");

    gSearchServiceInitStub.rejects(
      new Error("Simulating search init failures")
    );

    const result = await TopSites.getLinksWithDefaults();
    Assert.ok(result);

    gSearchServiceInitStub.resolves();

    sandbox.restore();
    await cleanup();
  }
);

add_task(async function test_getLinksWithDefaults() {
  NewTabUtils.activityStreamLinks.getTopSites.resetHistory();

  let sandbox = sinon.createSandbox();
  let cleanup = stubTopSites(sandbox);

  TopSites.refreshDefaults("https://foo.com");

  info("getLinksWithDefaults should get the links from NewTabUtils");
  let result = await TopSites.getLinksWithDefaults();

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
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_filterAdult() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should filter out non-pinned adult sites");

  sandbox.stub(FilterAdult, "filter").returns([]);
  const TEST_URL = "https://foo.com/";
  sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [{ url: TEST_URL }]);

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  const result = await TopSites.getLinksWithDefaults();
  Assert.ok(FilterAdult.filter.calledOnce);
  Assert.equal(result.length, 1);
  Assert.equal(result[0].url, TEST_URL);

  sandbox.restore();
  await cleanup();
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

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");
  const result = await TopSites.getLinksWithDefaults();

  // what we should be left with is just the top site we added, and not the default site we blocked
  Assert.equal(result.length, 1);
  Assert.deepEqual(result[0], topsite);
  let foundBlocked = result.find(site => site.url === blockedDefaultSite.url);
  Assert.ok(!foundBlocked, "Should not have found blocked site.");

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_dedupe() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should call dedupe.group on the links");
  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  let stub = sandbox.stub(TopSites.dedupe, "group").callsFake((...id) => id);
  await TopSites.getLinksWithDefaults();

  Assert.ok(stub.calledOnce, "dedupe.group was called once");
  sandbox.restore();
  await cleanup();
});

add_task(async function test__dedupe_key() {
  let sandbox = sinon.createSandbox();

  info("_dedupeKey should dedupe on hostname instead of url");
  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  let site = { url: "foo", hostname: "bar" };
  let result = TopSites._dedupeKey(site);

  Assert.equal(result, site.hostname, "deduped on hostname");
  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_adds_defaults() {
  let sandbox = sinon.createSandbox();

  info(
    "getLinksWithDefaults should add defaults if there are are not enough links"
  );
  const TEST_LINKS = [{ frecency: FAKE_FRECENCY, url: "foo.com" }];
  gGetTopSitesStub.resolves(TEST_LINKS);
  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  let result = await TopSites.getLinksWithDefaults();

  let reference = [...TEST_LINKS, ...DEFAULT_TOP_SITES].map(s =>
    Object.assign({}, s, {
      hostname: shortURL(s),
      typedBonus: true,
    })
  );

  Assert.deepEqual(result, reference);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
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

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");

    let result = await TopSites.getLinksWithDefaults();

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
    await cleanup();
  }
);

add_task(async function test_getLinksWithDefaults_no_throw_on_no_links() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should not throw if NewTabUtils returns null");
  gGetTopSitesStub.resolves(null);

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  await TopSites.getLinksWithDefaults();
  Assert.ok(true, "getLinksWithDefaults did not throw");

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
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

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  const TEST_ROWS = 3;
  TopSites.store.state.Prefs.values.topSitesRows = TEST_ROWS;

  let result = await TopSites.getLinksWithDefaults();
  Assert.equal(result.length, TEST_ROWS * TOP_SITES_MAX_SITES_PER_ROW);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_reuse_cache() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should reuse the cache on subsequent calls");

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  gGetTopSitesStub.resetHistory();

  await TopSites.getLinksWithDefaults();
  await TopSites.getLinksWithDefaults();

  Assert.ok(
    NewTabUtils.activityStreamLinks.getTopSites.calledOnce,
    "getTopSites only called once"
  );

  sandbox.restore();
  await cleanup();
});

add_task(
  async function test_getLinksWithDefaults_ignore_cache_on_requesting_more() {
    let sandbox = sinon.createSandbox();
    info("getLinksWithDefaults should ignore the cache when requesting more");

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");

    gGetTopSitesStub.resetHistory();

    await TopSites.getLinksWithDefaults();
    TopSites.store.state.Prefs.values.topSitesRows *= 3;
    await TopSites.getLinksWithDefaults();

    Assert.ok(
      NewTabUtils.activityStreamLinks.getTopSites.calledTwice,
      "getTopSites called twice"
    );

    sandbox.restore();
    await cleanup();
  }
);

add_task(
  async function test_getLinksWithDefaults_migrate_frecent_screenshot_data() {
    let sandbox = sinon.createSandbox();
    info(
      "getLinksWithDefaults should migrate frecent screenshot data without getting screenshots again"
    );

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");

    gGetTopSitesStub.resetHistory();

    TopSites.store.state.Prefs.values[SHOWN_ON_NEWTAB_PREF] = true;
    await TopSites.getLinksWithDefaults();

    let originalCallCount = Screenshots.getScreenshotForURL.callCount;
    TopSites.frecentCache.expire();

    let result = await TopSites.getLinksWithDefaults();

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
    await cleanup();
  }
);

add_task(
  async function test_getLinksWithDefaults_migrate_pinned_favicon_data() {
    let sandbox = sinon.createSandbox();
    info(
      "getLinksWithDefaults should migrate pinned favicon data without getting favicons again"
    );

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");

    gGetTopSitesStub.resetHistory();

    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "https://foo.com/" }]);

    await TopSites.getLinksWithDefaults();

    let originalCallCount =
      NewTabUtils.activityStreamProvider._addFavicons.callCount;
    TopSites.pinnedCache.expire();

    let result = await TopSites.getLinksWithDefaults();

    Assert.equal(
      NewTabUtils.activityStreamProvider._addFavicons.callCount,
      originalCallCount,
      "_addFavicons was not called again."
    );
    Assert.equal(result[0].favicon, FAKE_FAVICON);
    Assert.equal(result[0].faviconSize, FAKE_FAVICON_SIZE);

    sandbox.restore();
    await cleanup();
  }
);

add_task(async function test_getLinksWithDefaults_no_internal_properties() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should not expose internal link properties");

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  let result = await TopSites.getLinksWithDefaults();

  let internal = Object.keys(result[0]).filter(key => key.startsWith("__"));
  Assert.equal(internal.join(""), "");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_copy_frecent_screenshot() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should copy the screenshot of the frecent site if " +
      "pinned site doesn't have customScreenshotURL"
  );

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  const TEST_SCREENSHOT = "screenshot";

  gGetTopSitesStub.resolves([
    { url: "https://foo.com/", screenshot: TEST_SCREENSHOT },
  ]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com/" }]);

  let result = await TopSites.getLinksWithDefaults();

  Assert.equal(result[0].screenshot, TEST_SCREENSHOT);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_no_copy_frecent_screenshot() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should not copy the frecent screenshot if " +
      "customScreenshotURL is set"
  );

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  gGetTopSitesStub.resolves([
    { url: "https://foo.com/", screenshot: "screenshot" },
  ]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com/", customScreenshotURL: "custom" }]);

  let result = await TopSites.getLinksWithDefaults();

  Assert.equal(result[0].screenshot, undefined);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_persist_screenshot() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults should keep the same screenshot if no frecent site is found"
  );

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  const CUSTOM_SCREENSHOT = "custom";

  gGetTopSitesStub.resolves([]);
  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://foo.com/", screenshot: CUSTOM_SCREENSHOT }]);

  let result = await TopSites.getLinksWithDefaults();

  Assert.equal(result[0].screenshot, CUSTOM_SCREENSHOT);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(
  async function test_getLinksWithDefaults_no_overwrite_pinned_screenshot() {
    let sandbox = sinon.createSandbox();
    info("getLinksWithDefaults should not overwrite pinned site screenshot");

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");

    const EXISTING_SCREENSHOT = "some-screenshot";

    gGetTopSitesStub.resolves([{ url: "https://foo.com/", screenshot: "foo" }]);
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [
        { url: "https://foo.com/", screenshot: EXISTING_SCREENSHOT },
      ]);

    let result = await TopSites.getLinksWithDefaults();

    Assert.equal(result[0].screenshot, EXISTING_SCREENSHOT);

    gGetTopSitesStub.resolves(FAKE_LINKS);
    sandbox.restore();
    await cleanup();
  }
);

add_task(
  async function test_getLinksWithDefaults_no_searchTopSite_from_frecent() {
    let sandbox = sinon.createSandbox();
    info("getLinksWithDefaults should not set searchTopSite from frecent site");

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");

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

    let result = await TopSites.getLinksWithDefaults();

    Assert.ok(!result[0].searchTopSite);
    // But it should copy over other properties
    Assert.equal(result[0].screenshot, EXISTING_SCREENSHOT);

    gGetTopSitesStub.resolves(FAKE_LINKS);
    sandbox.restore();
    await cleanup();
  }
);

add_task(async function test_getLinksWithDefaults_concurrency_getTopSites() {
  let sandbox = sinon.createSandbox();
  info(
    "getLinksWithDefaults concurrent calls should call the backing data once"
  );

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  NewTabUtils.activityStreamLinks.getTopSites.resetHistory();

  await Promise.all([
    TopSites.getLinksWithDefaults(),
    TopSites.getLinksWithDefaults(),
  ]);

  Assert.ok(
    NewTabUtils.activityStreamLinks.getTopSites.calledOnce,
    "getTopSites only called once"
  );

  sandbox.restore();
  await cleanup();
});

add_task(
  async function test_getLinksWithDefaults_concurrency_getScreenshotForURL() {
    let sandbox = sinon.createSandbox();
    info(
      "getLinksWithDefaults concurrent calls should call the backing data once"
    );

    let cleanup = stubTopSites(sandbox);
    TopSites.refreshDefaults("https://foo.com");
    TopSites.store.state.Prefs.values[SHOWN_ON_NEWTAB_PREF] = true;

    NewTabUtils.activityStreamLinks.getTopSites.resetHistory();
    Screenshots.getScreenshotForURL.resetHistory();

    await Promise.all([
      TopSites.getLinksWithDefaults(),
      TopSites.getLinksWithDefaults(),
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
    await cleanup();

    cleanup = stubTopSites(sandbox);
    TopSites.store.state.Prefs.values[SHOWN_ON_NEWTAB_PREF] = true;

    TopSites.refreshDefaults("https://foo.com");

    sandbox.stub(TopSites, "_requestRichIcon");
    await Promise.all([
      TopSites.getLinksWithDefaults(),
      TopSites.getLinksWithDefaults(),
    ]);

    Assert.equal(
      TopSites.store.dispatch.callCount,
      FAKE_LINKS.length,
      "getLinksWithDefaults concurrent calls should dispatch once per link screenshot fetched"
    );

    sandbox.restore();
    await cleanup();
  }
);

add_task(async function test_getLinksWithDefaults_deduping_no_dedupe_pinned() {
  let sandbox = sinon.createSandbox();
  info("getLinksWithDefaults should not dedupe pinned sites");

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults("https://foo.com");

  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [
      { url: "https://developer.mozilla.org/en-US/docs/Web" },
      { url: "https://developer.mozilla.org/en-US/docs/Learn" },
    ]);

  let sites = await TopSites.getLinksWithDefaults();
  Assert.equal(sites.length, 2 * TOP_SITES_MAX_SITES_PER_ROW);
  Assert.equal(sites[0].url, NewTabUtils.pinnedLinks.links[0].url);
  Assert.equal(sites[1].url, NewTabUtils.pinnedLinks.links[1].url);
  Assert.equal(sites[0].hostname, sites[1].hostname);

  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_prefer_pinned_sites() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should prefer pinned sites over links");

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults();

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

  let sites = await TopSites.getLinksWithDefaults();

  // Expecting 3 links where there's 2 pinned and 1 www.mozilla.org, so
  // the frecent with matching hostname as pinned is removed.
  Assert.equal(sites.length, 3);
  Assert.equal(sites[0].url, NewTabUtils.pinnedLinks.links[0].url);
  Assert.equal(sites[1].url, NewTabUtils.pinnedLinks.links[1].url);
  Assert.equal(sites[2].url, SECOND_TOP_SITE_URL);

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_title_and_null() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should return sites that have a title");

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults();

  sandbox
    .stub(NewTabUtils.pinnedLinks, "links")
    .get(() => [{ url: "https://github.com/mozilla/activity-stream" }]);

  let sites = await TopSites.getLinksWithDefaults();
  for (let site of sites) {
    Assert.ok(site.hostname);
  }

  info("getLinksWithDefaults should not throw for null entries");
  sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [null]);
  await TopSites.getLinksWithDefaults();
  Assert.ok(true, "getLinksWithDefaults didn't throw");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_getLinksWithDefaults_calls__fetchIcon() {
  let sandbox = sinon.createSandbox();

  info("getLinksWithDefaults should return sites that have a title");

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults();

  sandbox.spy(TopSites, "_fetchIcon");
  let results = await TopSites.getLinksWithDefaults();
  Assert.ok(results.length, "Got back some results");
  Assert.equal(TopSites._fetchIcon.callCount, results.length);
  for (let result of results) {
    Assert.ok(TopSites._fetchIcon.calledWith(result));
  }

  sandbox.restore();
  await cleanup();
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

  let cleanup = stubTopSites(sandbox);
  TopSites.refreshDefaults();

  sandbox.stub(TopSites, "_fetchScreenshot");
  await TopSites.getLinksWithDefaults();

  Assert.ok(TopSites._fetchScreenshot.calledWith(sinon.match.object, "custom"));

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_init() {
  let sandbox = sinon.createSandbox();

  sandbox.stub(NimbusFeatures.newtab, "onUpdate");

  let cleanup = stubTopSites(sandbox);

  sandbox.stub(TopSites, "refresh");
  await TopSites.init();

  info("TopSites.init should call refresh (broadcast: true)");
  Assert.ok(TopSites.refresh.calledOnce, "refresh called once");
  Assert.ok(
    TopSites.refresh.calledWithExactly({
      broadcast: true,
      isStartup: true,
    })
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_refresh() {
  let sandbox = sinon.createSandbox();

  sandbox.stub(NimbusFeatures.newtab, "onUpdate");

  let cleanup = stubTopSites(sandbox);

  sandbox.stub(TopSites, "_fetchIcon");
  TopSites._startedUp = true;

  info("TopSites.refresh should wait for tippytop to initialize");
  TopSites._tippyTopProvider.initialized = false;
  sandbox.stub(TopSites._tippyTopProvider, "init").resolves();

  await TopSites.refresh();

  Assert.ok(
    TopSites._tippyTopProvider.init.calledOnce,
    "TopSites._tippyTopProvider.init called once"
  );

  info(
    "TopSites.refresh should not init the tippyTopProvider if already initialized"
  );
  TopSites._tippyTopProvider.initialized = true;
  TopSites._tippyTopProvider.init.resetHistory();

  await TopSites.refresh();

  Assert.ok(
    TopSites._tippyTopProvider.init.notCalled,
    "tippyTopProvider not initted again"
  );

  info("TopSites.refresh should broadcast TOP_SITES_UPDATED");
  TopSites.store.dispatch.resetHistory();
  sandbox.stub(TopSites, "getLinksWithDefaults").resolves([]);

  await TopSites.refresh({ broadcast: true });

  Assert.ok(TopSites.store.dispatch.calledOnce, "dispatch called once");
  Assert.ok(
    TopSites.store.dispatch.calledWithExactly(
      ac.BroadcastToContent({
        type: at.TOP_SITES_UPDATED,
        data: { links: [] },
      })
    )
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_refresh_dispatch() {
  let sandbox = sinon.createSandbox();

  info("TopSites.refresh should dispatch an action with the links returned");

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "_fetchIcon");
  TopSites._startedUp = true;

  await TopSites.refresh({ broadcast: true });
  let reference = FAKE_LINKS.map(site =>
    Object.assign({}, site, {
      hostname: shortURL(site),
      typedBonus: true,
    })
  );

  Assert.ok(TopSites.store.dispatch.calledOnce, "Store.dispatch called once");
  Assert.equal(
    TopSites.store.dispatch.firstCall.args[0].type,
    at.TOP_SITES_UPDATED
  );
  Assert.deepEqual(
    TopSites.store.dispatch.firstCall.args[0].data.links,
    reference
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_refresh_empty_slots() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSites.refresh should handle empty slots in the resulting top sites array"
  );

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "_fetchIcon");
  TopSites._startedUp = true;

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

  await TopSites.refresh({ broadcast: true });

  Assert.ok(TopSites.store.dispatch.calledOnce, "Store.dispatch called once");

  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_refresh_to_preloaded() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSites.refresh should dispatch AlsoToPreloaded when broadcast is false"
  );

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "_fetchIcon");
  TopSites._startedUp = true;

  gGetTopSitesStub.resolves([]);
  await TopSites.refresh({ broadcast: false });

  Assert.ok(TopSites.store.dispatch.calledOnce, "Store.dispatch called once");
  Assert.ok(
    TopSites.store.dispatch.calledWithExactly(
      ac.AlsoToPreloaded({
        type: at.TOP_SITES_UPDATED,
        data: { links: [] },
      })
    )
  );
  gGetTopSitesStub.resolves(FAKE_LINKS);
  sandbox.restore();
  await cleanup();
});

add_task(async function test_refresh_init_storage() {
  let sandbox = sinon.createSandbox();

  info("TopSites.refresh should not init storage of it's already initialized");

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "_fetchIcon");
  TopSites._startedUp = true;

  TopSites._storage.initialized = true;

  await TopSites.refresh({ broadcast: false });

  Assert.ok(
    TopSites._storage.init.notCalled,
    "TopSites._storage.init was not called."
  );
  sandbox.restore();
  await cleanup();
});

add_task(async function test_refresh_handles_indexedDB_errors() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSites.refresh should dispatch AlsoToPreloaded when broadcast is false"
  );

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "_fetchIcon");
  TopSites._startedUp = true;

  TopSites._storage.get.throws(new Error());

  try {
    await TopSites.refresh({ broadcast: false });
    Assert.ok(true, "refresh should have succeeded");
  } catch (e) {
    Assert.ok(false, "Should not have thrown");
  }

  sandbox.restore();
  await cleanup();
});

add_task(async function test_getScreenshotPreview() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSites.getScreenshotPreview should dispatch preview if request is succesful"
  );

  let cleanup = stubTopSites(sandbox);
  await TopSites.getScreenshotPreview("custom", 1234);

  Assert.ok(TopSites.store.dispatch.calledOnce);
  Assert.ok(
    TopSites.store.dispatch.calledWithExactly(
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
  await cleanup();
});

add_task(async function test_getScreenshotPreview() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSites.getScreenshotPreview should return empty string if request fails"
  );

  let cleanup = stubTopSites(sandbox);
  Screenshots.getScreenshotForURL.resolves(Promise.resolve(null));
  await TopSites.getScreenshotPreview("custom", 1234);

  Assert.ok(TopSites.store.dispatch.calledOnce);
  Assert.ok(
    TopSites.store.dispatch.calledWithExactly(
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
  await cleanup();
});

add_task(async function test_onAction_part_1() {
  let sandbox = sinon.createSandbox();

  info("TopSites.onAction should call getScreenshotPreview on PREVIEW_REQUEST");

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "getScreenshotPreview");
  TopSites.onAction({
    type: at.PREVIEW_REQUEST,
    data: { url: "foo" },
    meta: { fromTarget: 1234 },
  });

  Assert.ok(
    TopSites.getScreenshotPreview.calledOnce,
    "TopSites.getScreenshotPreview called once"
  );
  Assert.ok(TopSites.getScreenshotPreview.calledWithExactly("foo", 1234));

  info("TopSites.onAction should refresh on SYSTEM_TICK");
  sandbox.stub(TopSites, "refresh");
  TopSites.onAction({ type: at.SYSTEM_TICK });

  Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
  Assert.ok(TopSites.refresh.calledWithExactly({ broadcast: false }));

  info(
    "TopSites.onAction should call with correct parameters on TOP_SITES_PIN"
  );
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  sandbox.spy(TopSites, "pin");

  let pinAction = {
    type: at.TOP_SITES_PIN,
    data: { site: { url: "foo.com" }, index: 7 },
  };
  TopSites.onAction(pinAction);
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
    TopSites.pin.calledOnce,
    "TopSites.onAction should call pin on TOP_SITES_PIN"
  );

  info(
    "TopSites.onAction should unblock a previously blocked top site if " +
      "we are now adding it manually via 'Add a Top Site' option"
  );
  sandbox.stub(NewTabUtils.blockedLinks, "unblock");
  pinAction = {
    type: at.TOP_SITES_PIN,
    data: { site: { url: "foo.com" }, index: -1 },
  };
  TopSites.onAction(pinAction);
  Assert.ok(
    NewTabUtils.blockedLinks.unblock.calledWith({
      url: pinAction.data.site.url,
    })
  );

  info("TopSites.onAction should call insert on TOP_SITES_INSERT");
  sandbox.stub(TopSites, "insert");
  let addAction = {
    type: at.TOP_SITES_INSERT,
    data: { site: { url: "foo.com" } },
  };

  TopSites.onAction(addAction);
  Assert.ok(TopSites.insert.calledOnce, "TopSites.insert called once");

  info(
    "TopSites.onAction should call unpin with correct parameters " +
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
  TopSites.onAction(unpinAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.unpin.calledOnce,
    "NewTabUtils.pinnedLinks.unpin called once"
  );
  Assert.ok(NewTabUtils.pinnedLinks.unpin.calledWith(unpinAction.data.site));

  sandbox.restore();
  await cleanup();
});

add_task(async function test_onAction_part_2() {
  let sandbox = sinon.createSandbox();

  info(
    "TopSites.onAction should call refresh without a target if we clear " +
      "history with PLACES_HISTORY_CLEARED"
  );

  let cleanup = stubTopSites(sandbox);
  sandbox.stub(TopSites, "refresh");
  TopSites.onAction({ type: at.PLACES_HISTORY_CLEARED });

  Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
  Assert.ok(TopSites.refresh.calledWithExactly({ broadcast: true }));

  TopSites.refresh.resetHistory();

  info(
    "TopSites.onAction should call refresh without a target " +
      "if we remove a Topsite from history"
  );
  TopSites.onAction({ type: at.PLACES_LINKS_DELETED });

  Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
  Assert.ok(TopSites.refresh.calledWithExactly({ broadcast: true }));

  info("TopSites.onAction should call init on INIT action");
  TopSites.onAction({ type: at.PLACES_LINKS_DELETED });
  sandbox.stub(TopSites, "init");
  TopSites.onAction({ type: at.INIT });
  Assert.ok(TopSites.init.calledOnce, "TopSites.init called once");

  info("TopSites.onAction should call refresh on PLACES_LINK_BLOCKED action");
  TopSites.refresh.resetHistory();
  await TopSites.onAction({ type: at.PLACES_LINK_BLOCKED });
  Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
  Assert.ok(TopSites.refresh.calledWithExactly({ broadcast: true }));

  info("TopSites.onAction should call refresh on PLACES_LINKS_CHANGED action");
  TopSites.refresh.resetHistory();
  await TopSites.onAction({ type: at.PLACES_LINKS_CHANGED });
  Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
  Assert.ok(TopSites.refresh.calledWithExactly({ broadcast: false }));

  info(
    "TopSites.onAction should call pin with correct args on " +
      "TOP_SITES_INSERT without an index specified"
  );
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  let addAction = {
    type: at.TOP_SITES_INSERT,
    data: { site: { url: "foo.bar", label: "foo" } },
  };
  TopSites.onAction(addAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.pin.calledOnce,
    "NewTabUtils.pinnedLinks.pin called once"
  );
  Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(addAction.data.site, 0));

  info(
    "TopSites.onAction should call pin with correct args on " +
      "TOP_SITES_INSERT"
  );
  NewTabUtils.pinnedLinks.pin.resetHistory();
  let dropAction = {
    type: at.TOP_SITES_INSERT,
    data: { site: { url: "foo.bar", label: "foo" }, index: 3 },
  };
  TopSites.onAction(dropAction);
  Assert.ok(
    NewTabUtils.pinnedLinks.pin.calledOnce,
    "NewTabUtils.pinnedLinks.pin called once"
  );
  Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(dropAction.data.site, 3));

  // TopSites.init needs to actually run in order to register the observers that'll
  // be removed in the following UNINIT test, otherwise uninit will throw.
  TopSites.init.restore();
  TopSites.init();

  info("TopSites.onAction should remove the expiration filter on UNINIT");
  sandbox.stub(PageThumbs, "removeExpirationFilter");
  TopSites.onAction({ type: "UNINIT" });
  Assert.ok(
    PageThumbs.removeExpirationFilter.calledOnce,
    "PageThumbs.removeExpirationFilter called once"
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_onAction_part_3() {
  let sandbox = sinon.createSandbox();

  let cleanup = stubTopSites(sandbox);

  info(
    "TopSites.onAction should call updatePinnedSearchShortcuts " +
      "on UPDATE_PINNED_SEARCH_SHORTCUTS action"
  );
  sandbox.stub(TopSites, "updatePinnedSearchShortcuts");
  let addedShortcuts = [
    {
      url: "https://google.com",
      searchVendor: "google",
      label: "google",
      searchTopSite: true,
    },
  ];
  await TopSites.onAction({
    type: at.UPDATE_PINNED_SEARCH_SHORTCUTS,
    data: { addedShortcuts },
  });
  Assert.ok(
    TopSites.updatePinnedSearchShortcuts.calledOnce,
    "TopSites.updatePinnedSearchShortcuts called once"
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_insert_part_1() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  let cleanup = stubTopSites(sandbox);

  info("TopSites.insert should pin site in first slot of empty pinned list");
  Screenshots.getScreenshotForURL.resolves(Promise.resolve(null));
  await TopSites.getScreenshotPreview("custom", 1234);

  Assert.ok(TopSites.store.dispatch.calledOnce);
  Assert.ok(
    TopSites.store.dispatch.calledWithExactly(
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

  {
    info(
      "TopSites.insert should pin site in first slot of pinned list with " +
        "empty first slot"
    );

    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);
    let site = { url: "foo.bar", label: "foo" };
    await TopSites.insert({ data: { site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSites.insert should move a pinned site in first slot to the " +
        "next slot: part 1"
    );
    let site1 = { url: "example.com" };
    sandbox.stub(NewTabUtils.pinnedLinks, "links").get(() => [site1]);
    let site = { url: "foo.bar", label: "foo" };

    await TopSites.insert({ data: { site } });
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
      "TopSites.insert should move a pinned site in first slot to the " +
        "next slot: part 2"
    );
    let site1 = { url: "example.com" };
    let site2 = { url: "example.org" };
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [site1, null, site2]);
    let site = { url: "foo.bar", label: "foo" };
    await TopSites.insert({ data: { site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 1));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
  await cleanup();
});

add_task(async function test_insert_part_2() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  let cleanup = stubTopSites(sandbox);

  {
    info(
      "TopSites.insert should unpin the last site if all slots are " +
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
    TopSites.store.state.Prefs.values.topSitesRows = 1;
    let site = { url: "foo.bar", label: "foo" };
    await TopSites.insert({ data: { site } });
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
    info("TopSites.insert should trigger refresh on TOP_SITES_INSERT");
    sandbox.stub(TopSites, "refresh");
    let addAction = {
      type: at.TOP_SITES_INSERT,
      data: { site: { url: "foo.com" } },
    };

    await TopSites.insert(addAction);

    Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
  }

  {
    info("TopSites.insert should correctly handle different index values");
    let index = -1;
    let site = { url: "foo.bar", label: "foo" };
    let action = { data: { index, site } };

    await TopSites.insert(action);
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));

    index = undefined;
    await TopSites.insert(action);
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 0));

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
  await cleanup();
});

add_task(async function test_insert_part_3() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  let cleanup = stubTopSites(sandbox);

  {
    info("TopSites.insert should pin site in specified slot that is free");
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };

    await TopSites.insert({ data: { index: 2, site, draggedFromIndex: 0 } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info(
      "TopSites.insert should move a pinned site in specified slot " +
        "to the next slot"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };

    await TopSites.insert({ data: { index: 2, site, draggedFromIndex: 3 } });
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
      "TopSites.insert should move pinned sites in the direction " +
        "of the dragged site"
    );

    let site1 = { url: "foo.bar", label: "foo" };
    let site2 = { url: "example.com", label: "example" };
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, null, site2]);

    await TopSites.insert({
      data: { index: 2, site: site1, draggedFromIndex: 0 },
    });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 2));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site2, 1));
    NewTabUtils.pinnedLinks.pin.resetHistory();

    await TopSites.insert({
      data: { index: 2, site: site1, draggedFromIndex: 5 },
    });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledTwice,
      "NewTabUtils.pinnedLinks.pin called twice"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site1, 2));
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site2, 3));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  {
    info("TopSites.insert should not insert past the visible top sites");
    let site1 = { url: "foo.bar", label: "foo" };
    await TopSites.insert({
      data: { index: 42, site: site1, draggedFromIndex: 0 },
    });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.notCalled,
      "NewTabUtils.pinnedLinks.pin wasn't called"
    );

    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
  await cleanup();
});

add_task(async function test_pin_part_1() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  sandbox.spy(TopSites.pinnedCache, "request");
  let cleanup = stubTopSites(sandbox);

  {
    info("TopSites.pin should pin site in specified slot empty pinned list");
    let site = {
      url: "foo.bar",
      label: "foo",
      customScreenshotURL: "screenshot",
    };
    Assert.ok(
      TopSites.pinnedCache.request.notCalled,
      "TopSites.pinnedCache.request not called"
    );
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.called,
      "NewTabUtils.pinnedLinks.pin called"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    NewTabUtils.pinnedLinks.pin.resetHistory();
    TopSites.pinnedCache.request.resetHistory();
  }

  {
    info(
      "TopSites.pin should lookup the link object to update the custom " +
        "screenshot"
    );
    let site = {
      url: "foo.bar",
      label: "foo",
      customScreenshotURL: "screenshot",
    };
    Assert.ok(
      TopSites.pinnedCache.request.notCalled,
      "TopSites.pinnedCache.request not called"
    );
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      TopSites.pinnedCache.request.called,
      "TopSites.pinnedCache.request called"
    );
    NewTabUtils.pinnedLinks.pin.resetHistory();
    TopSites.pinnedCache.request.resetHistory();
  }

  {
    info(
      "TopSites.pin should lookup the link object to update the custom " +
        "screenshot when the custom screenshot is initially null"
    );
    let site = {
      url: "foo.bar",
      label: "foo",
      customScreenshotURL: null,
    };
    Assert.ok(
      TopSites.pinnedCache.request.notCalled,
      "TopSites.pinnedCache.request not called"
    );
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      TopSites.pinnedCache.request.called,
      "TopSites.pinnedCache.request called"
    );
    NewTabUtils.pinnedLinks.pin.resetHistory();
    TopSites.pinnedCache.request.resetHistory();
  }

  {
    info(
      "TopSites.pin should not do a link object lookup if custom " +
        "screenshot field is not set"
    );
    let site = { url: "foo.bar", label: "foo" };
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      !TopSites.pinnedCache.request.called,
      "TopSites.pinnedCache.request never called"
    );
    NewTabUtils.pinnedLinks.pin.resetHistory();
    TopSites.pinnedCache.request.resetHistory();
  }

  {
    info(
      "TopSites.pin should pin site in specified slot of pinned " +
        "list that is free"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    NewTabUtils.pinnedLinks.pin.resetHistory();
  }

  sandbox.restore();
  await cleanup();
});

add_task(async function test_pin_part_2() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");

  {
    info("TopSites.pin should save the searchTopSite attribute if set");
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo", searchTopSite: true };
    let cleanup = stubTopSites(sandbox);
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.firstCall.args[0].searchTopSite);
    NewTabUtils.pinnedLinks.pin.resetHistory();
    await cleanup();
  }

  {
    info(
      "TopSites.pin should NOT move a pinned site in specified " +
        "slot to the next slot"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [null, null, { url: "example.com" }]);

    let site = { url: "foo.bar", label: "foo" };
    let cleanup = stubTopSites(sandbox);
    await TopSites.pin({ data: { index: 2, site } });
    Assert.ok(
      NewTabUtils.pinnedLinks.pin.calledOnce,
      "NewTabUtils.pinnedLinks.pin called once"
    );
    Assert.ok(NewTabUtils.pinnedLinks.pin.calledWith(site, 2));
    NewTabUtils.pinnedLinks.pin.resetHistory();
    await cleanup();
  }

  {
    info(
      "TopSites.pin should properly update LinksCache object " +
        "properties between migrations"
    );
    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "https://foo.com/" }]);

    let cleanup = stubTopSites(sandbox);
    let pinnedLinks = await TopSites.pinnedCache.request();
    Assert.equal(pinnedLinks.length, 1);
    TopSites.pinnedCache.expire();

    pinnedLinks[0].__sharedCache.updateLink("screenshot", "foo");

    pinnedLinks = await TopSites.pinnedCache.request();
    Assert.equal(pinnedLinks[0].screenshot, "foo");

    // Force cache expiration in order to trigger a migration of objects
    TopSites.pinnedCache.expire();
    pinnedLinks[0].__sharedCache.updateLink("screenshot", "bar");

    pinnedLinks = await TopSites.pinnedCache.request();
    Assert.equal(pinnedLinks[0].screenshot, "bar");
    await cleanup();
  }

  sandbox.restore();
});

add_task(async function test_pin_part_3() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  sandbox.spy(TopSites, "insert");

  {
    info("TopSites.pin should call insert if index < 0");
    let site = { url: "foo.bar", label: "foo" };
    let action = { data: { index: -1, site } };
    let cleanup = stubTopSites(sandbox);
    await TopSites.pin(action);

    Assert.ok(TopSites.insert.calledOnce, "TopSites.insert called once");
    Assert.ok(TopSites.insert.calledWithExactly(action));
    NewTabUtils.pinnedLinks.pin.resetHistory();
    TopSites.insert.resetHistory();
    await cleanup();
  }

  {
    info("TopSites.pin should not call insert if index == 0");
    let site = { url: "foo.bar", label: "foo" };
    let action = { data: { index: 0, site } };
    let cleanup = stubTopSites(sandbox);
    await TopSites.pin(action);

    Assert.ok(!TopSites.insert.called, "TopSites.insert not called");
    NewTabUtils.pinnedLinks.pin.resetHistory();
    await cleanup();
  }

  {
    info("TopSites.pin should trigger refresh on TOP_SITES_PIN");
    let cleanup = stubTopSites(sandbox);
    sandbox.stub(TopSites, "refresh");
    let pinExistingAction = {
      type: at.TOP_SITES_PIN,
      data: { site: FAKE_LINKS[4], index: 4 },
    };

    await TopSites.pin(pinExistingAction);

    Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");
    NewTabUtils.pinnedLinks.pin.resetHistory();
    await cleanup();
  }

  sandbox.restore();
});

add_task(async function test_integration() {
  let sandbox = sinon.createSandbox();

  info("Test adding a pinned site and removing it with actions");
  let cleanup = stubTopSites(sandbox);

  let resolvers = [];
  TopSites.store.dispatch = sandbox.stub().callsFake(() => {
    resolvers.shift()();
  });
  TopSites._startedUp = true;
  sandbox.stub(TopSites, "_fetchScreenshot");

  let forDispatch = action =>
    new Promise(resolve => {
      resolvers.push(resolve);
      TopSites.onAction(action);
    });

  TopSites._requestRichIcon = sandbox.stub();
  let url = "https://pin.me";
  sandbox.stub(NewTabUtils.pinnedLinks, "pin").callsFake(link => {
    NewTabUtils.pinnedLinks.links.push(link);
  });

  await forDispatch({ type: at.TOP_SITES_INSERT, data: { site: { url } } });
  NewTabUtils.pinnedLinks.links.pop();
  await forDispatch({ type: at.PLACES_LINK_BLOCKED });

  Assert.ok(
    TopSites.store.dispatch.calledTwice,
    "TopSites.store.dispatch called twice"
  );
  Assert.equal(
    TopSites.store.dispatch.firstCall.args[0].data.links[0].url,
    url
  );
  Assert.equal(
    TopSites.store.dispatch.secondCall.args[0].data.links[0].url,
    FAKE_LINKS[0].url
  );

  sandbox.restore();
  await cleanup();
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
      "TopSites.getLinksWithDefaults should filter out alexa top 5 " +
        "search from the default sites"
    );
    let cleanup = stubTopSites(sandbox);
    TopSites.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;
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

    const urlsReturned = (await TopSites.getLinksWithDefaults()).map(
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
    await cleanup();
  }

  {
    info(
      "TopSites.getLinksWithDefaults should not filter out alexa, default " +
        "search from the query results if the experiment pref is off"
    );
    let cleanup = stubTopSites(sandbox);
    TopSites.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = false;

    gGetTopSitesStub.resolves([
      { url: "https://google.com" },
      { url: "https://foo.com" },
      { url: "https://duckduckgo" },
    ]);
    let urlsReturned = (await TopSites.getLinksWithDefaults()).map(
      link => link.url
    );

    Assert.ok(urlsReturned.includes("https://google.com"));
    gGetTopSitesStub.resolves(FAKE_LINKS);
    await cleanup();
  }

  {
    info(
      "TopSites.getLinksWithDefaults should filter out the current " +
        "default search from the default sites"
    );
    let cleanup = stubTopSites(sandbox);
    TopSites.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;

    sandbox.stub(TopSites, "_currentSearchHostname").get(() => "amazon");
    TopSites.onAction({
      type: at.PREFS_INITIAL_VALUES,
      data: { "default.sites": "google.com,amazon.com" },
    });
    gGetTopSitesStub.resolves([{ url: "https://foo.com" }]);

    let urlsReturned = (await TopSites.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(!urlsReturned.includes("https://amazon.com"));

    gGetTopSitesStub.resolves(FAKE_LINKS);
    await cleanup();
  }

  {
    info(
      "TopSites.getLinksWithDefaults should not filter out current " +
        "default search from pinned sites even if it matches the current " +
        "default search"
    );
    let cleanup = stubTopSites(sandbox);
    TopSites.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;

    sandbox
      .stub(NewTabUtils.pinnedLinks, "links")
      .get(() => [{ url: "google.com" }]);
    gGetTopSitesStub.resolves([{ url: "https://foo.com" }]);

    let urlsReturned = (await TopSites.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(urlsReturned.includes("google.com"));

    gGetTopSitesStub.resolves(FAKE_LINKS);
    await cleanup();
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

    sandbox.stub(TopSites, "refresh");

    {
      info(
        "TopSites.getLinksWithDefaults should call refresh and set " +
          "._currentSearchHostname to the new engine hostname when the " +
          "default search engine has been set"
      );
      let cleanup = stubTopSites(sandbox);
      TopSites.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;

      TopSites.observe(
        null,
        "browser-search-engine-modified",
        "engine-default"
      );
      Assert.equal(TopSites._currentSearchHostname, "duckduckgo");
      Assert.ok(TopSites.refresh.calledOnce, "TopSites.refresh called once");

      gGetTopSitesStub.resolves(FAKE_LINKS);
      TopSites.refresh.resetHistory();
      await cleanup();
    }

    {
      info(
        "TopSites.getLinksWithDefaults should call refresh when the " +
          "experiment pref has changed"
      );
      let cleanup = stubTopSites(sandbox);
      TopSites.store.state.Prefs.values[NO_DEFAULT_SEARCH_TILE_PREF] = true;

      TopSites.onAction({
        type: at.PREF_CHANGED,
        data: { name: NO_DEFAULT_SEARCH_TILE_PREF, value: true },
      });
      Assert.ok(
        TopSites.refresh.calledOnce,
        "TopSites.refresh was called once"
      );

      TopSites.onAction({
        type: at.PREF_CHANGED,
        data: { name: NO_DEFAULT_SEARCH_TILE_PREF, value: false },
      });
      Assert.ok(
        TopSites.refresh.calledTwice,
        "TopSites.refresh was called twice"
      );

      gGetTopSitesStub.resolves(FAKE_LINKS);
      TopSites.refresh.resetHistory();
      await cleanup();
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

  let prepTopSites = () => {
    TopSites.store.state.Prefs.values[SEARCH_SHORTCUTS_EXPERIMENT_PREF] = true;
    TopSites.store.state.Prefs.values[SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF] =
      "google,amazon";
    TopSites.store.state.Prefs.values[SEARCH_SHORTCUTS_HAVE_PINNED_PREF] = "";
  };

  {
    info(
      "TopSites should updateCustomSearchShortcuts when experiment " +
        "pref is turned on"
    );
    let cleanup = stubTopSites(sandbox);
    prepTopSites();
    TopSites.store.state.Prefs.values[SEARCH_SHORTCUTS_EXPERIMENT_PREF] = false;
    sandbox.spy(TopSites, "updateCustomSearchShortcuts");

    // turn the experiment on
    TopSites.onAction({
      type: at.PREF_CHANGED,
      data: { name: SEARCH_SHORTCUTS_EXPERIMENT_PREF, value: true },
    });

    Assert.ok(
      TopSites.updateCustomSearchShortcuts.calledOnce,
      "TopSites.updateCustomSearchShortcuts called once"
    );
    TopSites.updateCustomSearchShortcuts.restore();
    await cleanup();
  }

  {
    info(
      "TopSites should filter out default top sites that match a " +
        "hostname of a search shortcut if previously blocked"
    );
    let cleanup = stubTopSites(sandbox);
    prepTopSites();
    TopSites.refreshDefaults("https://amazon.ca");
    sandbox
      .stub(NewTabUtils.blockedLinks, "links")
      .value([{ url: "https://amazon.com" }]);
    sandbox.stub(NewTabUtils.blockedLinks, "isBlocked").callsFake(site => {
      return NewTabUtils.blockedLinks.links[0].url === site.url;
    });

    let urlsReturned = (await TopSites.getLinksWithDefaults()).map(
      link => link.url
    );
    Assert.ok(!urlsReturned.includes("https://amazon.ca"));
    await cleanup();
  }

  {
    info("TopSites should update frecent search topsite icon");
    let cleanup = stubTopSites(sandbox);
    prepTopSites();
    sandbox.stub(TopSites._tippyTopProvider, "processSite").callsFake(site => {
      site.tippyTopIcon = "icon.png";
      site.backgroundColor = "#fff";
      return site;
    });
    gGetTopSitesStub.resolves([{ url: "https://google.com" }]);

    let urlsReturned = await TopSites.getLinksWithDefaults();

    let defaultSearchTopsite = urlsReturned.find(
      s => s.url === "https://google.com"
    );
    Assert.ok(defaultSearchTopsite.searchTopSite);
    Assert.equal(defaultSearchTopsite.tippyTopIcon, "icon.png");
    Assert.equal(defaultSearchTopsite.backgroundColor, "#fff");
    gGetTopSitesStub.resolves(FAKE_LINKS);
    TopSites._tippyTopProvider.processSite.restore();
    await cleanup();
  }

  {
    info("TopSites should update default search topsite icon");
    let cleanup = stubTopSites(sandbox);
    prepTopSites();
    sandbox.stub(TopSites._tippyTopProvider, "processSite").callsFake(site => {
      site.tippyTopIcon = "icon.png";
      site.backgroundColor = "#fff";
      return site;
    });
    gGetTopSitesStub.resolves([{ url: "https://foo.com" }]);
    TopSites.onAction({
      type: at.PREFS_INITIAL_VALUES,
      data: { "default.sites": "google.com,amazon.com" },
    });

    let urlsReturned = await TopSites.getLinksWithDefaults();

    let defaultSearchTopsite = urlsReturned.find(
      s => s.url === "https://amazon.com"
    );
    Assert.ok(defaultSearchTopsite.searchTopSite);
    Assert.equal(defaultSearchTopsite.tippyTopIcon, "icon.png");
    Assert.equal(defaultSearchTopsite.backgroundColor, "#fff");
    gGetTopSitesStub.resolves(FAKE_LINKS);
    TopSites._tippyTopProvider.processSite.restore();
    TopSites.store.dispatch.resetHistory();
    await cleanup();
  }

  {
    info(
      "TopSites should dispatch UPDATE_SEARCH_SHORTCUTS on " +
        "updateCustomSearchShortcuts"
    );
    let cleanup = stubTopSites(sandbox);
    prepTopSites();
    TopSites.store.state.Prefs.values[
      "improvesearch.noDefaultSearchTile"
    ] = true;
    await TopSites.updateCustomSearchShortcuts();
    Assert.ok(
      TopSites.store.dispatch.calledOnce,
      "TopSites.store.dispatch called once"
    );
    Assert.ok(
      TopSites.store.dispatch.calledWith({
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
    await cleanup();
  }

  sandbox.restore();
});

add_task(async function test_updatePinnedSearchShortcuts() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(NewTabUtils.pinnedLinks, "pin");
  sandbox.stub(NewTabUtils.pinnedLinks, "unpin");

  {
    info(
      "TopSites.updatePinnedSearchShortcuts should unpin a " +
        "shortcut in deletedShortcuts"
    );
    let cleanup = stubTopSites(sandbox);

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
    TopSites.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });
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
    await cleanup();
  }

  {
    info(
      "TopSites.updatePinnedSearchShortcuts should pin a shortcut " +
        "in addedShortcuts"
    );
    let cleanup = stubTopSites(sandbox);

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
    TopSites.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });
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
    await cleanup();
  }

  {
    info(
      "TopSites.updatePinnedSearchShortcuts should pin and unpin " +
        "in the same action"
    );
    let cleanup = stubTopSites(sandbox);

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
    TopSites.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });

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
    await cleanup();
  }

  {
    info(
      "TopSites.updatePinnedSearchShortcuts should pin a shortcut in " +
        "addedShortcuts even if pinnedLinks is full"
    );
    let cleanup = stubTopSites(sandbox);

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
    TopSites.updatePinnedSearchShortcuts({ addedShortcuts, deletedShortcuts });

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
    await cleanup();
  }

  sandbox.restore();
});
