/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PAGE_BASENAME = "contentSearchUI.html";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearchUI.js";
const TEST_ENGINE_PREFIX = "browser_searchSuggestionEngine";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_ENGINE_2_BASENAME = "searchSuggestionEngine2.xml";

const TEST_MSG = "ContentSearchUIControllerTest";

requestLongerTimeout(2);

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
  checkState(state, "xfoo", [], -1);

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

add_task(function* tabKey() {
  yield setUp();
  yield msg("key", { key: "x", waitForSuggestions: true });

  let state = yield msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = yield msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = yield msg("key", { key: "VK_TAB", modifiers: { shiftKey: true }});
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = yield msg("key", { key: "VK_TAB", modifiers: { shiftKey: true }});
  checkState(state, "x", [], -1);

  yield setUp();

  yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });

  for (let i = 0; i < 3; ++i) {
    state = yield msg("key", "VK_TAB");
  }
  checkState(state, "x", [], -1);

  yield setUp();

  yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = yield msg("key", "VK_TAB");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0, 0);

  state = yield msg("key", "VK_TAB");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0, 1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = yield msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = yield msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = yield msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = yield msg("key", "VK_TAB");
  checkState(state, "xbar", [], -1);

  yield msg("reset");
});

add_task(function* cycleSuggestions() {
  yield setUp();
  yield msg("key", { key: "x", waitForSuggestions: true });

  let cycle = Task.async(function* (aSelectedButtonIndex) {
    let modifiers = {
      shiftKey: true,
      accelKey: true,
    };

    let state = yield msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xbar", ["xfoo", "xbar"], 1, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_UP", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_UP", modifiers });
    checkState(state, "xbar", ["xfoo", "xbar"], 1, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_UP", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = yield msg("key", { key: "VK_UP", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);
  });

  yield cycle();

  // Repeat with a one-off selected.
  let state = yield msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 2);
  yield cycle(0);

  // Repeat with the settings button selected.
  state = yield msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 3);
  yield cycle(1);

  yield msg("reset");
});

add_task(function* cycleOneOffs() {
  yield setUp();
  yield msg("key", { key: "x", waitForSuggestions: true });

  yield msg("addDuplicateOneOff");

  let state = yield msg("key", "VK_DOWN");
  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  let modifiers = {
    altKey: true,
  };

  state = yield msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = yield msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = yield msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = yield msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = yield msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = yield msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  // If the settings button is selected, pressing alt+up/down should select the
  // last/first one-off respectively (and deselect the settings button).
  yield msg("key", "VK_TAB");
  yield msg("key", "VK_TAB");
  state = yield msg("key", "VK_TAB"); // Settings button selected.
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 2);

  state = yield msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = yield msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 2);

  state = yield msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  yield msg("removeLastOneOff");
  yield msg("reset");
});

add_task(function* mouse() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("mousemove", 0);
  checkState(state, "x", ["xfoo", "xbar"], 0);

  state = yield msg("mousemove", 1);
  checkState(state, "x", ["xfoo", "xbar"], 1);

  state = yield msg("mousemove", 2);
  checkState(state, "x", ["xfoo", "xbar"], 1, 0);

  state = yield msg("mousemove", 3);
  checkState(state, "x", ["xfoo", "xbar"], 1, 1);

  state = yield msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], 1);

  yield msg("reset");
  yield setUp();

  state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = yield msg("mousemove", 0);
  checkState(state, "x", ["xfoo", "xbar"], 0);

  state = yield msg("mousemove", 2);
  checkState(state, "x", ["xfoo", "xbar"], 0, 0);

  state = yield msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], 0);

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

add_task(function* cycleEngines() {
  yield setUp();
  yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });

  let promiseEngineChange = function(newEngineName) {
    let deferred = Promise.defer();
    Services.obs.addObserver(function resolver(subj, topic, data) {
      if (data != "engine-current") {
        return;
      }
      SimpleTest.is(subj.name, newEngineName, "Engine cycled correctly");
      Services.obs.removeObserver(resolver, "browser-search-engine-modified");
      deferred.resolve();
    }, "browser-search-engine-modified", false);
    return deferred.promise;
  }

  let p = promiseEngineChange(TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME);
  yield msg("key", { key: "VK_DOWN", modifiers: { accelKey: true }});
  yield p;

  p = promiseEngineChange(TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME);
  yield msg("key", { key: "VK_UP", modifiers: { accelKey: true }});
  yield p;

  yield msg("reset");
});

add_task(function* search() {
  yield setUp();

  let modifiers = {};
  ["altKey", "ctrlKey", "metaKey", "shiftKey"].forEach(k => modifiers[k] = true);

  // Test typing a query and pressing enter.
  let p = msg("waitForSearch");
  yield msg("key", { key: "x", waitForSuggestions: true });
  yield msg("key", { key: "VK_RETURN", modifiers });
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
  yield msg("key", { key: "VK_RETURN", modifiers });
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
  yield msg("key", { key: "VK_RETURN", modifiers });
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
  yield msg("click", { eltIdx: -1, modifiers });
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
  yield msg("click", { eltIdx: 1, modifiers });
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
  yield msg("click", { eltIdx: 3, modifiers });
  mesg = yield p;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  delete eventData.selection;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Test selecting a suggestion, then clicking a one-off without deselecting the
  // suggestion.
  yield msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  yield msg("mousemove", 1);
  yield msg("mousemove", 3);
  yield msg("click", { eltIdx: 3, modifiers });
  mesg = yield p;
  eventData.searchString = "xfoo"
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  yield promiseTab();
  yield setUp();

  // Same as above, but with the keyboard.
  delete modifiers.button;
  yield msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  yield msg("key", "VK_DOWN");
  yield msg("key", "VK_DOWN");
  yield msg("key", "VK_TAB");
  yield msg("key", { key: "VK_RETURN", modifiers });
  mesg = yield p;
  eventData.selection = {
    index: 1,
    kind: "key",
  };
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
  yield msg("key", { key: "VK_RETURN", modifiers });
  mesg = yield p;
  eventData.searchString = "x"
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  delete eventData.selection;
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
  p = msg("waitForSearch");
  yield msg("click", { eltIdx: 1, modifiers });
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

var gDidInitialSetUp = false;

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

function msg(type, data = null) {
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type,
    data,
  });
  let deferred = Promise.defer();
  gMsgMan.addMessageListener(TEST_MSG, function onMsg(msgObj) {
    if (msgObj.data.type != type) {
      return;
    }
    gMsgMan.removeMessageListener(TEST_MSG, onMsg);
    deferred.resolve(msgObj.data.data);
  });
  return deferred.promise;
}

function checkState(actualState, expectedInputVal, expectedSuggestions,
                    expectedSelectedIdx, expectedSelectedButtonIdx) {
  expectedSuggestions = expectedSuggestions.map(sugg => {
    return typeof(sugg) == "object" ? sugg : {
      str: sugg,
      type: "remote",
    };
  });

  if (expectedSelectedIdx == -1 && expectedSelectedButtonIdx != undefined) {
    expectedSelectedIdx = expectedSuggestions.length + expectedSelectedButtonIdx;
  }

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
  if (expectedSelectedButtonIdx != undefined) {
    expectedState.selectedButtonIndex = expectedSelectedButtonIdx;
  } else if (expectedSelectedIdx < expectedSuggestions.length) {
    expectedState.selectedButtonIndex = -1;
  } else {
    expectedState.selectedButtonIndex = expectedSelectedIdx - expectedSuggestions.length;
  }

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
  msgMan.addMessageListener(name, function onMsg(msgObj) {
    info("Received " + name + " message " + msgObj.data.type + "\n");
    if (msgObj.data.type == type) {
      msgMan.removeMessageListener(name, onMsg);
      deferred.resolve(msgObj);
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
    let engine1 = yield promiseNewSearchEngine(TEST_ENGINE_BASENAME);
    yield promiseNewSearchEngine(TEST_ENGINE_2_BASENAME);
    Services.search.currentEngine = engine1;
    for (let engine of currentEngines) {
      Services.search.removeEngine(engine);
    }
    registerCleanupFunction(() => {
      Services.search.restoreDefaultEngines();
      Services.search.currentEngine = Services.search.getEngineByName(currentEngineName);
    });
  });
}
