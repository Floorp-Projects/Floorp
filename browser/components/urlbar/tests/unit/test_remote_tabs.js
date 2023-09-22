/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 */
"use strict";

const { Weave } = ChromeUtils.importESModule(
  "resource://services-sync/main.sys.mjs"
);

// A mock "Tabs" engine which autocomplete will use instead of the real
// engine. We pass a constructor that Sync creates.
function MockTabsEngine() {
  this.clients = null; // We'll set this dynamically
}

MockTabsEngine.prototype = {
  name: "tabs",

  startTracking() {},
  getAllClients() {
    return this.clients;
  },
};

// A clients engine that doesn't need to be a constructor.
let MockClientsEngine = {
  getClientType(guid) {
    Assert.ok(guid.endsWith("desktop") || guid.endsWith("mobile"));
    return guid.endsWith("mobile") ? "phone" : "desktop";
  },
  remoteClientExists(id) {
    return true;
  },
  getClientName(id) {
    return id.endsWith("mobile") ? "My Phone" : "My Desktop";
  },
};

// Configure the singleton engine for a test.
function configureEngine(clients) {
  // Configure the instance Sync created.
  let engine = Weave.Service.engineManager.get("tabs");
  engine.clients = clients;
  Weave.Service.clientsEngine = MockClientsEngine;
  // Send an observer that pretends the engine just finished a sync.
  Services.obs.notifyObservers(null, "weave:engine:sync:finish", "tabs");
}

testEngine_setup();

add_setup(async function () {
  // Tell Sync about the mocks.
  Weave.Service.engineManager.register(MockTabsEngine);

  // Tell the Sync XPCOM service it is initialized.
  let weaveXPCService = Cc["@mozilla.org/weave/service;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  weaveXPCService.ready = true;

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("services.sync.username");
    Services.prefs.clearUserPref("services.sync.registerEngines");
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
    await cleanupPlaces();
  });

  Services.prefs.setCharPref("services.sync.username", "someone@somewhere.com");
  Services.prefs.setCharPref("services.sync.registerEngines", "");
  // Avoid hitting the network.
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
});

add_task(async function test_minimal() {
  // The minimal client and tabs info we can get away with.
  configureEngine([
    {
      id: "desktop",
      tabs: [
        {
          urlHistory: ["http://example.com/"],
        },
      ],
    },
  ]);

  let query = "ex";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://example.com/",
        device: "My Desktop",
      }),
    ],
  });
});

add_task(async function test_maximal() {
  // Every field that could possibly exist on a remote record.
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: ["http://example.com/"],
          title: "An Example",
          icon: "http://favicon",
        },
      ],
    },
  ]);

  let query = "ex";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://example.com/",
        device: "My Phone",
        title: "An Example",
        iconUri: "moz-anno:favicon:http://favicon/",
      }),
    ],
  });
});

add_task(async function test_noShowIcons() {
  Services.prefs.setBoolPref("services.sync.syncedTabs.showRemoteIcons", false);
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: ["http://example.com/"],
          title: "An Example",
          icon: "http://favicon",
        },
      ],
    },
  ]);

  let query = "ex";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://example.com/",
        device: "My Phone",
        title: "An Example",
        // expecting the default favicon due to that pref.
        iconUri: "",
      }),
    ],
  });
  Services.prefs.clearUserPref("services.sync.syncedTabs.showRemoteIcons");
});

add_task(async function test_dontMatchSyncedTabs() {
  Services.prefs.setBoolPref("services.sync.syncedTabs.showRemoteTabs", false);
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: ["http://example.com/"],
          title: "An Example",
          icon: "http://favicon",
        },
      ],
    },
  ]);

  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  Services.prefs.clearUserPref("services.sync.syncedTabs.showRemoteTabs");
});

add_task(async function test_tabsDisabledInUrlbar() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.remotetab", false);
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: ["http://example.com/"],
          title: "An Example",
          icon: "http://favicon",
        },
      ],
    },
  ]);

  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  Services.prefs.clearUserPref("browser.urlbar.suggest.remotetab");
});

add_task(async function test_matches_title() {
  // URL doesn't match search expression, should still match the title.
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: ["http://foo.com/"],
          title: "An Example",
        },
      ],
    },
  ]);

  let query = "ex";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.com/",
        device: "My Phone",
        title: "An Example",
      }),
    ],
  });
});

add_task(async function test_localtab_matches_override() {
  // We have an open tab to the same page on a remote device, only "switch to
  // tab" should appear as duplicate detection removed the remote one.

  // First set up Sync to have the page as a remote tab.
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: ["http://foo.com/"],
          title: "An Example",
        },
      ],
    },
  ]);

  // Set up Places to think the tab is open locally.
  let uri = Services.io.newURI("http://foo.com/");
  await PlacesTestUtils.addVisits([{ uri, title: "An Example" }]);
  await addOpenPages(uri, 1);

  let query = "ex";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://foo.com/",
        title: "An Example",
      }),
    ],
  });

  await removeOpenPages(uri, 1);
  await cleanupPlaces();
});

add_task(async function test_remotetab_matches_override() {
  // If we have an history result to the same page, we should only get the
  // remote tab match.
  let url = "http://foo.remote.com/";
  // First set up Sync to have the page as a remote tab.
  configureEngine([
    {
      id: "mobile",
      tabs: [
        {
          urlHistory: [url],
          title: "An Example",
        },
      ],
    },
  ]);

  // Set up Places to think the tab is in history.
  await PlacesTestUtils.addVisits(url);

  let query = "ex";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/",
        device: "My Phone",
        title: "An Example",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_mixed_result_types() {
  // In case we have many results, non-remote results should flex to the bottom.
  let url = "http://foo.remote.com/";
  let tabs = Array(6)
    .fill(0)
    .map((e, i) => ({
      urlHistory: [`${url}${i}`],
      title: "A title",
      lastUsed: Math.floor(Date.now() / 1000) - i * 86400, // i days ago.
    }));
  // First set up Sync to have the page as a remote tab.
  configureEngine([{ id: "mobile", tabs }]);

  // Register the page as an open tab.
  let openTabUrl = url + "openpage/";
  let uri = Services.io.newURI(openTabUrl);
  await PlacesTestUtils.addVisits([{ uri, title: "An Example" }]);
  await addOpenPages(uri, 1);

  // Also add a local history result.
  let historyUrl = url + "history/";
  await PlacesTestUtils.addVisits(historyUrl);

  let query = "rem";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/0",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[0].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/1",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[1].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/2",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[2].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/3",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[3].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/4",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[4].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/5",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[5].lastUsed,
      }),
      makeVisitResult(context, {
        uri: historyUrl,
        title: "test visit for " + historyUrl,
      }),
      makeTabSwitchResult(context, {
        uri: openTabUrl,
        title: "An Example",
      }),
    ],
  });
  await removeOpenPages(uri, 1);
  await cleanupPlaces();
});

add_task(async function test_many_remotetab_results() {
  let url = "http://foo.remote.com/";
  let tabs = Array(8)
    .fill(0)
    .map((e, i) => ({
      urlHistory: [`${url}${i}`],
      title: "A title",
      lastUsed: Math.floor(Date.now() / 1000) - i * 86400, // i days old.
    }));

  // First set up Sync to have the page as a remote tab.
  configureEngine([
    {
      id: "mobile",
      tabs,
    },
  ]);

  let query = "rem";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/0",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[0].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/1",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[1].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/2",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[2].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/3",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[3].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/4",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[4].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/5",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[5].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/6",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[6].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/7",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[7].lastUsed,
      }),
    ],
  });
});

add_task(async function multiple_clients() {
  let url = "http://foo.remote.com/";
  let mobileTabs = Array(2)
    .fill(0)
    .map((e, i) => ({
      urlHistory: [`${url}mobile/${i}`],
      lastUsed: Date.now() / 1000 - 4 * 86400, // 4 days old (past threshold)
    }));

  let desktopTabs = Array(3)
    .fill(0)
    .map((e, i) => ({
      urlHistory: [`${url}desktop/${i}`],
      lastUsed: Date.now() / 1000 - 1, // Fresh tabs
    }));

  // mobileTabs has the most recent tab, making it the most recent client. The
  // rest of its tabs are stale. The tabs in desktopTabs are fresh, but not
  // as fresh as the most recent tab in mobileTab.
  mobileTabs.push({
    urlHistory: [`${url}mobile/fresh`],
    lastUsed: Date.now() / 1000,
  });

  configureEngine([
    {
      id: "mobile",
      tabs: mobileTabs,
    },
    {
      id: "desktop",
      tabs: desktopTabs,
    },
  ]);

  // We expect that we will show the recent tab from mobileTabs, then all the
  // tabs from desktopTabs, then the remaining tabs from mobileTabs.
  let query = "rem";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/mobile/fresh",
        device: "My Phone",
        lastUsed: mobileTabs[2].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/desktop/0",
        device: "My Desktop",
        lastUsed: desktopTabs[0].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/desktop/1",
        device: "My Desktop",
        lastUsed: desktopTabs[1].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/desktop/2",
        device: "My Desktop",
        lastUsed: desktopTabs[2].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/mobile/0",
        device: "My Phone",
        lastUsed: mobileTabs[0].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/mobile/1",
        device: "My Phone",
        lastUsed: mobileTabs[1].lastUsed,
      }),
    ],
  });
});

add_task(async function test_restrictionCharacter() {
  let url = "http://foo.remote.com/";
  let tabs = Array(5)
    .fill(0)
    .map((e, i) => ({
      urlHistory: [`${url}${i}`],
      title: "A title",
      lastUsed: Math.floor(Date.now() / 1000) - i,
    }));
  configureEngine([
    {
      id: "mobile",
      tabs,
    },
  ]);

  // Also add an open page.
  let openTabUrl = url + "openpage/";
  let uri = Services.io.newURI(openTabUrl);
  await PlacesTestUtils.addVisits([{ uri, title: "An Example" }]);
  await addOpenPages(uri, 1);

  // We expect the open tab to flex to the bottom.
  let query = UrlbarTokenizer.RESTRICT.OPENPAGE;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/0",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[0].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/1",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[1].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/2",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[2].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/3",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[3].lastUsed,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/4",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[4].lastUsed,
      }),
      makeTabSwitchResult(context, {
        uri: openTabUrl,
        title: "An Example",
      }),
    ],
  });
  await removeOpenPages(uri, 1);
  await cleanupPlaces();
});

add_task(async function test_duplicate_remote_tabs() {
  let url = "http://foo.remote.com/";
  let tabs = Array(3)
    .fill(0)
    .map((e, i) => ({
      urlHistory: [url],
      title: "A title",
      lastUsed: Math.floor(Date.now() / 1000),
    }));
  configureEngine([
    {
      id: "mobile",
      tabs,
    },
  ]);

  // We expect the duplicate tabs to be deduped.
  let query = UrlbarTokenizer.RESTRICT.OPENPAGE;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeRemoteTabResult(context, {
        uri: "http://foo.remote.com/",
        device: "My Phone",
        title: "A title",
        lastUsed: tabs[0].lastUsed,
      }),
    ],
  });
});
