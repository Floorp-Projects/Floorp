/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures that the about:home startup cache worker
 * script can correctly convert a state object from the Activity
 * Stream Redux store into an HTML document and script.
 */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

SearchTestUtils.init(this);
AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const { AboutNewTab } = ChromeUtils.import(
  "resource:///modules/AboutNewTab.jsm"
);
const { PREFS_CONFIG } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStream.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BasePromiseWorker",
  "resource://gre/modules/PromiseWorker.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser"]);

const CACHE_WORKER_URL = "resource://activity-stream/lib/cache-worker.js";
const NEWTAB_RENDER_URL =
  "resource://activity-stream/data/content/newtab-render.js";

/**
 * In order to make this test less brittle, much of Activity Stream is
 * initialized here in order to generate a state object at runtime, rather
 * than hard-coding one in. This requires quite a bit of machinery in order
 * to work properly. Specifically, we need to launch an HTTP server to serve
 * a dynamic layout, and then have that layout point to a local feed rather
 * than one from the Pocket CDN.
 */
add_task(async function setup() {
  do_get_profile();
  // The SearchService is also needed in order to construct the initial state,
  // which means that the AddonManager needs to be available.
  await AddonTestUtils.promiseStartupManager();

  // The example.com domain will be used to host the dynamic layout JSON and
  // the top stories JSON.
  let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
  server.registerDirectory("/", do_get_cwd());

  // Top Stories are disabled by default in our testing profiles.
  Services.prefs.setBoolPref(
    "browser.newtabpage.activity-stream.feeds.section.topstories",
    true
  );
  Services.prefs.setBoolPref(
    "browser.newtabpage.activity-stream.feeds.system.topstories",
    true
  );

  let defaultDSConfig = JSON.parse(
    PREFS_CONFIG.get("discoverystream.config").getValue({
      geo: "US",
      locale: "en-US",
    })
  );

  let newConfig = Object.assign(defaultDSConfig, {
    show_spocs: false,
    hardcoded_layout: false,
    layout_endpoint: "http://example.com/ds_layout.json",
  });

  // Configure Activity Stream to query for the layout JSON file that points
  // at the local top stories feed.
  Services.prefs.setCharPref(
    "browser.newtabpage.activity-stream.discoverystream.config",
    JSON.stringify(newConfig)
  );

  // We need to allow example.com as a place to get both the layout and the
  // top stories from.
  Services.prefs.setCharPref(
    "browser.newtabpage.activity-stream.discoverystream.endpoints",
    `http://example.com`
  );

  Services.prefs.setBoolPref(
    "browser.newtabpage.activity-stream.telemetry.structuredIngestion",
    false
  );
  Services.prefs.setBoolPref("browser.ping-centre.telemetry", false);

  // We need a default search engine set up for rendering the search input.
  await SearchTestUtils.installSearchExtension({
    name: "Test engine",
    keyword: "@testengine",
    search_url_get_params: "s={searchTerms}",
  });
  Services.search.defaultEngine = Services.search.getEngineByName(
    "Test engine"
  );

  // Initialize Activity Stream, and pretend that a new window has been loaded
  // to kick off initializing all of the feeds.
  AboutNewTab.init();
  AboutNewTab.onBrowserReady();

  // Much of Activity Stream initializes asynchronously. This is the easiest way
  // I could find to ensure that enough of the feeds had initialized to produce
  // a meaningful cached document.
  await TestUtils.waitForCondition(() => {
    let feed = AboutNewTab.activityStream.store.feeds.get(
      "feeds.discoverystreamfeed"
    );
    return feed?.loaded;
  });
});

/**
 * Gets the Activity Stream Redux state from Activity Stream and sends it
 * into an instance of the cache worker to ensure that the resulting markup
 * and script makes sense.
 */
add_task(async function test_cache_worker() {
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
  });

  let state = AboutNewTab.activityStream.store.getState();

  let cacheWorker = new BasePromiseWorker(CACHE_WORKER_URL);
  let { page, script } = await cacheWorker.post("construct", [state]);
  ok(!!page.length, "Got page content");
  ok(!!script.length, "Got script content");

  // The template strings should have been replaced.
  equal(
    page.indexOf("{{ MARKUP }}"),
    -1,
    "Page template should have {{ MARKUP }} replaced"
  );
  equal(
    page.indexOf("{{ CACHE_TIME }}"),
    -1,
    "Page template should have {{ CACHE_TIME }} replaced"
  );
  equal(
    script.indexOf("{{ STATE }}"),
    -1,
    "Script template should have {{ STATE }} replaced"
  );

  // Now let's make sure that the generated script makes sense. We'll
  // evaluate it in a sandbox to make sure broken JS doesn't break the
  // test.
  let sandbox = Cu.Sandbox(Cu.getGlobalForObject({}));
  let passedState = null;

  // window.NewtabRenderUtils.renderCache is the exposed API from
  // activity-stream.jsx that the script is expected to call to hydrate
  // the pre-rendered markup. We'll implement that, and use that to ensure
  // that the passed in state object matches the state we sent into the
  // worker.
  sandbox.window = {
    NewtabRenderUtils: {
      renderCache(aState) {
        passedState = aState;
      },
    },
  };
  Cu.evalInSandbox(script, sandbox);

  // The NEWTAB_RENDER_URL script is what ultimately causes the state
  // to be passed into the renderCache function.
  Services.scriptloader.loadSubScript(NEWTAB_RENDER_URL, sandbox);

  equal(
    sandbox.window.__FROM_STARTUP_CACHE__,
    true,
    "Should have set __FROM_STARTUP_CACHE__ to true"
  );

  // The worker is expected to modify the state slightly before running
  // it through ReactDOMServer by setting App.isForStartupCache to true.
  // This allows React components to change their behaviour if the cache
  // is being generated.
  state.App.isForStartupCache = true;

  // Some of the properties on the state might have values set to undefined.
  // There is no way to express a named undefined property on an object in
  // JSON, so we filter those out by stringifying and re-parsing.
  state = JSON.parse(JSON.stringify(state));

  Assert.deepEqual(
    passedState,
    state,
    "Should have called renderCache with the expected state"
  );

  // Now let's do a quick smoke-test on the markup to ensure that the
  // one Top Story from topstories.json is there.
  let parser = new DOMParser();
  let doc = parser.parseFromString(page, "text/html");
  let root = doc.getElementById("root");
  ok(root.childElementCount, "There are children on the root node");

  // There should be the 1 top story, and 2 placeholders.
  equal(
    Array.from(root.querySelectorAll(".ds-card")).length,
    3,
    "There are 3 DSCards"
  );
  let cardHostname = doc.querySelector("[data-section-id='topstories'] .source")
    .innerText;
  equal(cardHostname, "bbc.com", "Card hostname is bbc.com");

  let placeholders = doc.querySelectorAll(".ds-card.placeholder");
  equal(placeholders.length, 2, "There should be 2 placeholders");
});

/**
 * Tests that if the cache-worker construct method throws an exception
 * that the construct Promise still resolves. Passing a null state should
 * be enough to get it to throw.
 */
add_task(async function test_cache_worker_exception() {
  let cacheWorker = new BasePromiseWorker(CACHE_WORKER_URL);
  let { page, script } = await cacheWorker.post("construct", [null]);
  equal(page, null, "Should have gotten a null page nsIInputStream");
  equal(script, null, "Should have gotten a null script nsIInputStream");
});
