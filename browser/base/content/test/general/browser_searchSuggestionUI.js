/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PAGE_BASENAME = "searchSuggestionUI.html";
const TEST_CONTENT_SCRIPT_BASENAME = "searchSuggestionUI.js";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

const TEST_MSG = "SearchSuggestionUIControllerTest";

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

add_task(function* arrowKeys() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle down the suggestions starting from no selection.
  state = yield msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle up starting from no selection.
  state = yield msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = yield msg("key", "VK_UP");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = yield msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield msg("reset");
});

// The right arrow and return key function the same.
function rightArrowOrReturn(keyName) {
  return function* rightArrowOrReturnTest() {
    yield setUp();

    let state = yield msg("key", { key: "x", waitForSuggestions: true });
    checkState(state, "x", ["xfoo", "xbar"], -1);

    state = yield msg("key", "VK_DOWN");
    checkState(state, "xfoo", ["xfoo", "xbar"], 0);

    // This should make the xfoo suggestion sticky.  To make sure it sticks,
    // trigger suggestions again and cycle through them by pressing Down until
    // nothing is selected again.
    state = yield msg("key", keyName);
    checkState(state, "xfoo", [], -1);

    state = yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });
    checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

    state = yield msg("key", "VK_DOWN");
    checkState(state, "xfoofoo", ["xfoofoo", "xfoobar"], 0);

    state = yield msg("key", "VK_DOWN");
    checkState(state, "xfoobar", ["xfoofoo", "xfoobar"], 1);

    state = yield msg("key", "VK_DOWN");
    checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

    yield msg("reset");
  };
}

add_task(rightArrowOrReturn("VK_RIGHT"));
add_task(rightArrowOrReturn("VK_RETURN"));

add_task(function* mouse() {
  yield setUp();

  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Mouse over the first suggestion.
  state = yield msg("mousemove", 0);
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  // Mouse over the second suggestion.
  state = yield msg("mousemove", 1);
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  // Click the second suggestion.  This should make it sticky.  To make sure it
  // sticks, trigger suggestions again and cycle through them by pressing Down
  // until nothing is selected again.
  state = yield msg("mousedown", 1);
  checkState(state, "xbar", [], -1);

  state = yield msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "xbar", ["xbarfoo", "xbarbar"], -1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbarfoo", ["xbarfoo", "xbarbar"], 0);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbarbar", ["xbarfoo", "xbarbar"], 1);

  state = yield msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xbarfoo", "xbarbar"], -1);

  yield msg("reset");
});

add_task(function* formHistory() {
  yield setUp();

  // Type an X and add it to form history.
  let state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);
  yield msg("addInputValueToFormHistory");

  // Wait for Satchel to say it's been added to form history.
  let deferred = Promise.defer();
  Services.obs.addObserver(function onAdd(subj, topic, data) {
    if (data == "formhistory-add") {
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed", false);
  yield deferred.promise;

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

  state = yield msg("key", "VK_DELETE");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Wait for Satchel.
  deferred = Promise.defer();
  Services.obs.addObserver(function onAdd(subj, topic, data) {
    if (data == "formhistory-remove") {
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed", false);
  yield deferred.promise;

  // Reset the input.
  state = yield msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should still be gone.
  state = yield msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  yield msg("reset");
});


let gDidInitialSetUp = false;

function setUp() {
  return Task.spawn(function* () {
    if (!gDidInitialSetUp) {
      yield promiseNewEngine(TEST_ENGINE_BASENAME);
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
    gMsgMan.removeMessageListener(TEST_MSG, onMsg);
    deferred.resolve(msg.data);
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
    tableChildrenLength: expectedSuggestions.length,
    tableChildren: expectedSuggestions.map((s, i) => {
      let expectedClasses = new Set([s.type]);
      if (i == expectedSelectedIdx) {
        expectedClasses.add("selected");
      }
      return {
        textContent: s.str,
        classes: expectedClasses,
      };
    }),

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
      deferred.resolve();
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

function promiseNewEngine(basename) {
  info("Waiting for engine to be added: " + basename);
  let addDeferred = Promise.defer();
  let url = getRootDirectory(gTestPath) + basename;
  Services.search.addEngine(url, Ci.nsISearchEngine.TYPE_MOZSEARCH, "", false, {
    onSuccess: function (engine) {
      info("Search engine added: " + basename);
      registerCleanupFunction(() => Services.search.removeEngine(engine));
      addDeferred.resolve(engine);
    },
    onError: function (errCode) {
      ok(false, "addEngine failed with error code " + errCode);
      addDeferred.reject();
    },
  });
  return addDeferred.promise;
}
