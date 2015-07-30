/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PAGE_BASENAME = "contentSearchUI.html";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearchUI.js";
const TEST_ENGINE_PREFIX = "browser_searchSuggestionEngine";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_ENGINE_2_BASENAME = "searchSuggestionEngine2.xml";

const TEST_MSG = "ContentSearchUIControllerTest";

add_task(function* emptyInput() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("key", "VK_BACK_SPACE");
  checkState(state, "", [], -1);

  yield msg("reset");
});

add_task(function* blur() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("blur");
  checkState(state, "x", [], -1);

  yield msg("reset");
});

add_task(function* upDownKeys() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle down the suggestions starting from no selection.
  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle up starting from no selection.
  state = yield msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = yield msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = yield msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = yield msg("key", "VK_UP");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = yield msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield msg("reset");
});

add_task(function* rightLeftKeys() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("key", "VK_LEFT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("key", "VK_LEFT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("key", "VK_RIGHT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("key", "VK_RIGHT");
  checkState(state, "x", [], -1);

  state = yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  // This should make the xfoo suggestion sticky.  To make sure it sticks,
  // trigger suggestions again and cycle through them by pressing Down until
  // nothing is selected again.
  state = yield msg("key", "VK_RIGHT");
  checkState(state, "xfoo", [], 0);

  state = yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoofoo", ["xfoofoo", "xfoobar"], 0);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoobar", ["xfoofoo", "xfoobar"], 1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], 2);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], 3);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

  yield msg("reset");
});

add_task(function* mouse() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  for (let i = 0; i < 4; ++i) {
    state = yield msg("mousemove", i);
    checkState(state, "x", ["xfoo", "xbar"], i);
  }

  state = yield msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield msg("reset");
});

add_task(function* formHistory() {
  yield setUp();

  // Type an X and add it to form history.
  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);
  // Wait for Satchel to say it's been added to form history.
  let deferred = Promise.defer();
  Services.obs.addObserver(function onAdd(subj, topic, data) {
    if (data == "formhistory-add") {
      Services.obs.removeObserver(onAdd, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed", false);
  yield Promise.all([msg("addInputValueToFormHistory"), deferred.promise]);

  // Reset the input.
  state = yield msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should appear.
  state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", [{ str: "x", type: "formHistory" }, "xfoo", "xbar"],
             -1);

  // Select the form history entry and delete it.
  state = yield msg("key", "VK_DOWN");
  checkState(state, "x", [{ str: "x", type: "formHistory" }, "xfoo", "xbar"],
             0);

  // Wait for Satchel.
  deferred = Promise.defer();
  Services.obs.addObserver(function onRemove(subj, topic, data) {
    if (data == "formhistory-remove") {
      Services.obs.removeObserver(onRemove, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed", false);

  state = yield msg("key", "VK_DELETE");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield deferred.promise;

  // Reset the input.
  state = yield msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should still be gone.
  state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield msg("reset");
});

add_task(function* search() {
  yield setUp();

  let modifiers = {};
  ["altKey", "ctrlKey", "metaKey", "shiftKey"].forEach(k => modifiers[k] = true);

  // Test typing a query and pressing enter.
  let p = msg("waitForSearch");
  yield msg("key", { key: "x", waitForSuggestions: true });
  yield msg("key", { key: "VK_RETURN", modifiers: modifiers });
  let mesg = yield p;
  let eventData = {
    engineName: TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME,
    searchString: "x",
    healthReportKey: "test",
    searchPurpose: "test",
    originalEvent: modifiers,
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test typing a query, then selecting a suggestion and pressing enter.
  p = msg("waitForSearch");
  yield msg("key", { key: "x", waitForSuggestions: true });
  yield msg("key", "VK_DOWN");
  yield msg("key", "VK_DOWN");
  yield msg("key", { key: "VK_RETURN", modifiers: modifiers });
  mesg = yield p;
  eventData.searchString = "xfoo";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  eventData.selection = {
    index: 1,
    kind: "key",
  }
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test typing a query, then selecting a one-off button and pressing enter.
  p = msg("waitForSearch");
  yield msg("key", { key: "x", waitForSuggestions: true });
  yield msg("key", "VK_UP");
  yield msg("key", "VK_UP");
  yield msg("key", { key: "VK_RETURN", modifiers: modifiers });
  mesg = yield p;
  delete eventData.selection;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test typing a query and clicking the search engine header.
  p = msg("waitForSearch");
  modifiers.button = 0;
  yield msg("key", { key: "x", waitForSuggestions: true });
  yield msg("mousemove", -1);
  yield msg("click", { eltIdx: -1, modifiers: modifiers });
  mesg = yield p;
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test typing a query and then clicking a suggestion.
  yield msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  yield msg("mousemove", 1);
  yield msg("click", { eltIdx: 1, modifiers: modifiers });
  mesg = yield p;
  eventData.searchString = "xfoo";
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test typing a query and then clicking a one-off button.
  yield msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  yield msg("mousemove", 3);
  yield msg("click", { eltIdx: 3, modifiers: modifiers });
  mesg = yield p;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  delete eventData.selection;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test searching when using IME composition.
  let state = yield msg("startComposition", { data: "" });
  checkState(state, "", [], -1);
  state = yield msg("changeComposition", { data: "x", waitForSuggestions: true });
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], -1);
  yield msg("commitComposition");
  delete modifiers.button;
  p = msg("waitForSearch");
  yield msg("key", { key: "VK_RETURN", modifiers: modifiers });
  mesg = yield p;
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  state = yield msg("startComposition", { data: "" });
  checkState(state, "", [], -1);
  state = yield msg("changeComposition", { data: "x", waitForSuggestions: true });
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], -1);

  // Mouse over the first suggestion.
  state = yield msg("mousemove", 0);
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], 0);

  // Mouse over the second suggestion.
  state = yield msg("mousemove", 1);
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], 1);

  modifiers.button = 0;
  let currentTab = gBrowser.selectedTab;
  p = msg("waitForSearch");
  yield msg("click", { eltIdx: 1, modifiers: modifiers });
  mesg = yield p;
  eventData.searchString = "xfoo";
  eventData.originalEvent = modifiers;
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Remove form history entries.
  // Wait for Satchel.
  let deferred = Promise.defer();
  let historyCount = 2;
  Services.obs.addObserver(function onRemove(subj, topic, data) {
    if (data == "formhistory-remove") {
      if (--historyCount) {
        return;
      }
      Services.obs.removeObserver(onRemove, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed", false);

  yield msg("key", { key: "x", waitForSuggestions: true });
  yield msg("key", "VK_DOWN");
  yield msg("key", "VK_DOWN");
  yield msg("key", "VK_DELETE");
  yield msg("key", "VK_DOWN");
  yield msg("key", "VK_DELETE");
  yield deferred.promise;

  yield msg("reset");
  state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield promiseTab();
  yield setUp();
  yield msg("reset");
});

add_task(function* settings() {
  yield setUp();
  yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  yield msg("key", "VK_UP");
  let p = msg("waitForSearchSettings");
  yield msg("key", "VK_RETURN");
  yield p;

  yield msg("reset");
});

let gDidInitialSetUp = false;

function setUp(aNoEngine) {
  return Task.spawn(function* () {
    if (!gDidInitialSetUp) {
      Cu.import("resource:///modules/ContentSearch.jsm");
      let originalOnMessageSearch = ContentSearch._onMessageSearch;
      let originalOnMessageManageEngines = ContentSearch._onMessageManageEngines;
      ContentSearch._onMessageSearch = () => {};
      ContentSearch._onMessageManageEngines = () => {};
      registerCleanupFunction(() => {
        ContentSearch._onMessageSearch = originalOnMessageSearch;
        ContentSearch._onMessageManageEngines = originalOnMessageManageEngines;
      });
      yield setUpEngines();
      yield promiseTab();
      gDidInitialSetUp = true;
    }
    yield msg("focus");
  });
}

function msg(type, data=null) {
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: type,
    data: data,
  });
  let deferred = Promise.defer();
  gMsgMan.addMessageListener(TEST_MSG, function onMsg(msg) {
    if (msg.data.type != type) {
      return;
    }
    gMsgMan.removeMessageListener(TEST_MSG, onMsg);
    deferred.resolve(msg.data.data);
  });
  return deferred.promise;
}

function checkState(actualState, expectedInputVal, expectedSuggestions,
                    expectedSelectedIdx) {
  expectedSuggestions = expectedSuggestions.map(sugg => {
    return typeof(sugg) == "object" ? sugg : {
      str: sugg,
      type: "remote",
    };
  });

  let expectedState = {
    selectedIndex: expectedSelectedIdx,
    numSuggestions: expectedSuggestions.length,
    suggestionAtIndex: expectedSuggestions.map(s => s.str),
    isFormHistorySuggestionAtIndex:
      expectedSuggestions.map(s => s.type == "formHistory"),

    tableHidden: expectedSuggestions.length == 0,

    inputValue: expectedInputVal,
    ariaExpanded: expectedSuggestions.length == 0 ? "false" : "true",
  };

  SimpleTest.isDeeply(actualState, expectedState, "State");
}

var gMsgMan;

function promiseTab() {
  let deferred = Promise.defer();
  let tab = gBrowser.addTab();
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  gBrowser.selectedTab = tab;
  let pageURL = getRootDirectory(gTestPath) + TEST_PAGE_BASENAME;
  tab.linkedBrowser.addEventListener("load", function onLoad(event) {
    tab.linkedBrowser.removeEventListener("load", onLoad, true);
    gMsgMan = tab.linkedBrowser.messageManager;
    gMsgMan.sendAsyncMessage("ContentSearch", {
      type: "AddToWhitelist",
      data: [pageURL],
    });
    promiseMsg("ContentSearch", "AddToWhitelistAck", gMsgMan).then(() => {
      let jsURL = getRootDirectory(gTestPath) + TEST_CONTENT_SCRIPT_BASENAME;
      gMsgMan.loadFrameScript(jsURL, false);
      deferred.resolve(msg("init"));
    });
  }, true, true);
  openUILinkIn(pageURL, "current");
  return deferred.promise;
}

function promiseMsg(name, type, msgMan) {
  let deferred = Promise.defer();
  info("Waiting for " + name + " message " + type + "...");
  msgMan.addMessageListener(name, function onMsg(msg) {
    info("Received " + name + " message " + msg.data.type + "\n");
    if (msg.data.type == type) {
      msgMan.removeMessageListener(name, onMsg);
      deferred.resolve(msg);
    }
  });
  return deferred.promise;
}

function setUpEngines() {
  return Task.spawn(function* () {
    info("Removing default search engines");
    let currentEngineName = Services.search.currentEngine.name;
    let currentEngines = Services.search.getVisibleEngines();
    info("Adding test search engines");
    let engine1 = yield promiseNewEngine(TEST_ENGINE_BASENAME);
    let engine2 = yield promiseNewEngine(TEST_ENGINE_2_BASENAME);
    Services.search.currentEngine = engine1;
    for (let engine of currentEngines) {
      Services.search.removeEngine(engine);
    }
    registerCleanupFunction(() => {
      Services.search.restoreDefaultEngines();
      Services.search.removeEngine(engine1);
      Services.search.removeEngine(engine2);
      Services.search.currentEngine = Services.search.getEngineByName(currentEngineName);
    });
  });
}

function promiseNewEngine(basename) {
  info("Waiting for engine to be added: " + basename);
  let addDeferred = Promise.defer();
  let url = getRootDirectory(gTestPath) + basename;
  Services.search.addEngine(url, Ci.nsISearchEngine.TYPE_MOZSEARCH, "", false, {
    onSuccess: function (engine) {
      info("Search engine added: " + basename);
      addDeferred.resolve(engine);
    },
    onError: function (errCode) {
      ok(false, "addEngine failed with error code " + errCode);
      addDeferred.reject();
    },
  });
  return addDeferred.promise;
}
