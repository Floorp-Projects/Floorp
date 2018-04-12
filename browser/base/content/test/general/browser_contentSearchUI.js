/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PAGE_BASENAME = "contentSearchUI.html";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearchUI.js";
const TEST_ENGINE_PREFIX = "browser_searchSuggestionEngine";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_ENGINE_2_BASENAME = "searchSuggestionEngine2.xml";

const TEST_MSG = "ContentSearchUIControllerTest";

let {SearchTestUtils} = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm", {});

SearchTestUtils.init(Assert, registerCleanupFunction);

requestLongerTimeout(2);

add_task(async function emptyInput() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_BACK_SPACE");
  checkState(state, "", [], -1);

  await msg("reset");
});

add_task(async function blur() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("blur");
  checkState(state, "x", [], -1);

  await msg("reset");
});

add_task(async function upDownKeys() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle down the suggestions starting from no selection.
  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle up starting from no selection.
  state = await msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = await msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", "VK_UP");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = await msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
});

add_task(async function rightLeftKeys() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_LEFT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_LEFT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_RIGHT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_RIGHT");
  checkState(state, "x", [], -1);

  state = await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  // This should make the xfoo suggestion sticky.  To make sure it sticks,
  // trigger suggestions again and cycle through them by pressing Down until
  // nothing is selected again.
  state = await msg("key", "VK_RIGHT");
  checkState(state, "xfoo", [], -1);

  state = await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoofoo", ["xfoofoo", "xfoobar"], 0);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoobar", ["xfoofoo", "xfoobar"], 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], 2);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], 3);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

  await msg("reset");
});

add_task(async function tabKey() {
  await setUp();
  await msg("key", { key: "x", waitForSuggestions: true });

  let state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = await msg("key", { key: "VK_TAB", modifiers: { shiftKey: true }});
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", { key: "VK_TAB", modifiers: { shiftKey: true }});
  checkState(state, "x", [], -1);

  await setUp();

  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });

  for (let i = 0; i < 3; ++i) {
    state = await msg("key", "VK_TAB");
  }
  checkState(state, "x", [], -1);

  await setUp();

  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = await msg("key", "VK_TAB");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0, 0);

  state = await msg("key", "VK_TAB");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0, 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", [], -1);

  await msg("reset");
});

add_task(async function cycleSuggestions() {
  await setUp();
  await msg("key", { key: "x", waitForSuggestions: true });

  let cycle = async function(aSelectedButtonIndex) {
    let modifiers = {
      shiftKey: true,
      accelKey: true,
    };

    let state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xbar", ["xfoo", "xbar"], 1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "xbar", ["xfoo", "xbar"], 1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);
  };

  await cycle();

  // Repeat with a one-off selected.
  let state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 2);
  await cycle(0);

  // Repeat with the settings button selected.
  state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 3);
  await cycle(1);

  await msg("reset");
});

add_task(async function cycleOneOffs() {
  await setUp();
  await msg("key", { key: "x", waitForSuggestions: true });

  await msg("addDuplicateOneOff");

  let state = await msg("key", "VK_DOWN");
  state = await msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  let modifiers = {
    altKey: true,
  };

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  // If the settings button is selected, pressing alt+up/down should select the
  // last/first one-off respectively (and deselect the settings button).
  await msg("key", "VK_TAB");
  await msg("key", "VK_TAB");
  state = await msg("key", "VK_TAB"); // Settings button selected.
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 2);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 2);

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  await msg("removeLastOneOff");
  await msg("reset");
});

add_task(async function mouse() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("mousemove", 0);
  checkState(state, "x", ["xfoo", "xbar"], 0);

  state = await msg("mousemove", 1);
  checkState(state, "x", ["xfoo", "xbar"], 1);

  state = await msg("mousemove", 2);
  checkState(state, "x", ["xfoo", "xbar"], 2, 0);

  state = await msg("mousemove", 3);
  checkState(state, "x", ["xfoo", "xbar"], 3, 1);

  state = await msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
  await setUp();

  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("mousemove", 0);
  checkState(state, "x", ["xfoo", "xbar"], 0);

  state = await msg("mousemove", 2);
  checkState(state, "x", ["xfoo", "xbar"], 2, 0);

  state = await msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
});

add_task(async function formHistory() {
  await setUp();

  // Type an X and add it to form history.
  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);
  // Wait for Satchel to say it's been added to form history.
  let deferred = PromiseUtils.defer();
  Services.obs.addObserver(function onAdd(subj, topic, data) {
    if (data == "formhistory-add") {
      Services.obs.removeObserver(onAdd, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed");
  await Promise.all([msg("addInputValueToFormHistory"), deferred.promise]);

  // Reset the input.
  state = await msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should appear.
  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", [{ str: "x", type: "formHistory" }, "xfoo", "xbar"],
             -1);

  // Select the form history entry and delete it.
  state = await msg("key", "VK_DOWN");
  checkState(state, "x", [{ str: "x", type: "formHistory" }, "xfoo", "xbar"],
             0);

  // Wait for Satchel.
  deferred = PromiseUtils.defer();
  Services.obs.addObserver(function onRemove(subj, topic, data) {
    if (data == "formhistory-remove") {
      Services.obs.removeObserver(onRemove, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed");

  state = await msg("key", "VK_DELETE");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await deferred.promise;

  // Reset the input.
  state = await msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should still be gone.
  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
});

add_task(async function cycleEngines() {
  await setUp();
  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });

  let promiseEngineChange = function(newEngineName) {
    return new Promise(resolve => {
      Services.obs.addObserver(function resolver(subj, topic, data) {
        if (data != "engine-current") {
          return;
        }
        subj.QueryInterface(Ci.nsISearchEngine);
        SimpleTest.is(subj.name, newEngineName, "Engine cycled correctly");
        Services.obs.removeObserver(resolver, "browser-search-engine-modified");
        resolve();
      }, "browser-search-engine-modified");
    });
  };

  let p = promiseEngineChange(TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME);
  await msg("key", { key: "VK_DOWN", modifiers: { accelKey: true }});
  await p;

  p = promiseEngineChange(TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME);
  await msg("key", { key: "VK_UP", modifiers: { accelKey: true }});
  await p;

  await msg("reset");
});

add_task(async function search() {
  await setUp();

  let modifiers = {};
  ["altKey", "ctrlKey", "metaKey", "shiftKey"].forEach(k => modifiers[k] = true);

  // Test typing a query and pressing enter.
  let p = msg("waitForSearch");
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", { key: "VK_RETURN", modifiers });
  let mesg = await p;
  let eventData = {
    engineName: TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME,
    searchString: "x",
    healthReportKey: "test",
    searchPurpose: "test",
    originalEvent: modifiers,
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query, then selecting a suggestion and pressing enter.
  p = msg("waitForSearch");
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DOWN");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  eventData.selection = {
    index: 1,
    kind: "key",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query, then selecting a one-off button and pressing enter.
  p = msg("waitForSearch");
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", "VK_UP");
  await msg("key", "VK_UP");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  delete eventData.selection;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query and clicking the search engine header.
  p = msg("waitForSearch");
  modifiers.button = 0;
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("mousemove", -1);
  await msg("click", { eltIdx: -1, modifiers });
  mesg = await p;
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query and then clicking a suggestion.
  await msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  await msg("mousemove", 1);
  await msg("click", { eltIdx: 1, modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query and then clicking a one-off button.
  await msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  await msg("mousemove", 3);
  await msg("click", { eltIdx: 3, modifiers });
  mesg = await p;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  delete eventData.selection;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test selecting a suggestion, then clicking a one-off without deselecting the
  // suggestion, using the keyboard.
  delete modifiers.button;
  await msg("key", { key: "x", waitForSuggestions: true });
  p = msg("waitForSearch");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_TAB");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.selection = {
    index: 1,
    kind: "key",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test searching when using IME composition.
  let state = await msg("startComposition", { data: "" });
  checkState(state, "", [], -1);
  state = await msg("changeComposition", { data: "x", waitForSuggestions: true });
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], -1);
  await msg("commitComposition");
  delete modifiers.button;
  p = msg("waitForSearch");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  eventData.searchString = "x";
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  delete eventData.selection;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  state = await msg("startComposition", { data: "" });
  checkState(state, "", [], -1);
  state = await msg("changeComposition", { data: "x", waitForSuggestions: true });
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], -1);

  // Mouse over the first suggestion.
  state = await msg("mousemove", 0);
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], 0);

  // Mouse over the second suggestion.
  state = await msg("mousemove", 1);
  checkState(state, "x", [{ str: "x", type: "formHistory" },
                          { str: "xfoo", type: "formHistory" }, "xbar"], 1);

  modifiers.button = 0;
  p = msg("waitForSearch");
  await msg("click", { eltIdx: 1, modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.originalEvent = modifiers;
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Remove form history entries.
  // Wait for Satchel.
  let deferred = PromiseUtils.defer();
  let historyCount = 2;
  Services.obs.addObserver(function onRemove(subj, topic, data) {
    if (data == "formhistory-remove") {
      if (--historyCount) {
        return;
      }
      Services.obs.removeObserver(onRemove, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed");

  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DELETE");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DELETE");
  await deferred.promise;

  await msg("reset");
  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await promiseTab();
  await setUp();
  await msg("reset");
});

add_task(async function settings() {
  await setUp();
  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  await msg("key", "VK_UP");
  let p = msg("waitForSearchSettings");
  await msg("key", "VK_RETURN");
  await p;

  await msg("reset");
});

var gDidInitialSetUp = false;

function setUp(aNoEngine) {
  return (async function() {
    if (!gDidInitialSetUp) {
      ChromeUtils.import("resource:///modules/ContentSearch.jsm");
      let originalOnMessageSearch = ContentSearch._onMessageSearch;
      let originalOnMessageManageEngines = ContentSearch._onMessageManageEngines;
      ContentSearch._onMessageSearch = () => {};
      ContentSearch._onMessageManageEngines = () => {};
      registerCleanupFunction(() => {
        ContentSearch._onMessageSearch = originalOnMessageSearch;
        ContentSearch._onMessageManageEngines = originalOnMessageManageEngines;
      });
      await setUpEngines();
      await promiseTab();
      gDidInitialSetUp = true;
    }
    await msg("focus");
  })();
}

function msg(type, data = null) {
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type,
    data,
  });
  return new Promise(resolve => {
    gMsgMan.addMessageListener(TEST_MSG, function onMsg(msgObj) {
      if (msgObj.data.type != type) {
        return;
      }
      gMsgMan.removeMessageListener(TEST_MSG, onMsg);
      resolve(msgObj.data.data);
    });
  });
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

async function promiseTab() {
  let deferred = PromiseUtils.defer();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(() => BrowserTestUtils.removeTab(tab));
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
  openTrustedLinkIn(pageURL, "current");
  return deferred.promise;
}

function promiseMsg(name, type, msgMan) {
  return new Promise(resolve => {
    info("Waiting for " + name + " message " + type + "...");
    msgMan.addMessageListener(name, function onMsg(msgObj) {
      info("Received " + name + " message " + msgObj.data.type + "\n");
      if (msgObj.data.type == type) {
        msgMan.removeMessageListener(name, onMsg);
        resolve(msgObj);
      }
    });
  });
}

function setUpEngines() {
  return (async function() {
    info("Removing default search engines");
    let currentEngineName = Services.search.currentEngine.name;
    let currentEngines = Services.search.getVisibleEngines();
    info("Adding test search engines");
    let rootDir = getRootDirectory(gTestPath);
    let engine1 = await SearchTestUtils.promiseNewSearchEngine(
      rootDir + TEST_ENGINE_BASENAME);
    await SearchTestUtils.promiseNewSearchEngine(
      rootDir + TEST_ENGINE_2_BASENAME);
    Services.search.currentEngine = engine1;
    for (let engine of currentEngines) {
      Services.search.removeEngine(engine);
    }
    registerCleanupFunction(() => {
      Services.search.restoreDefaultEngines();
      Services.search.currentEngine = Services.search.getEngineByName(currentEngineName);
    });
  })();
}
