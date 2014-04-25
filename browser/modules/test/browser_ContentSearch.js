/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const CONTENT_SEARCH_MSG = "ContentSearch";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearch.js";

function generatorTest() {
  // nextStep() drives the iterator returned by this function.  This function's
  // iterator in turn drives the iterator of each test below.
  let currentTestIter = yield startNextTest();
  let arg = undefined;
  while (currentTestIter) {
    try {
      currentTestIter.send(arg);
      arg = yield null;
    }
    catch (err if err instanceof StopIteration) {
      currentTestIter = yield startNextTest();
      arg = undefined;
    }
  }
}

function startNextTest() {
  if (!gTests.length) {
    setTimeout(() => nextStep(null), 0);
    return;
  }
  let nextTestGen = gTests.shift();
  let nextTestIter = nextTestGen();
  addTab(() => {
    info("Starting test " + nextTestGen.name);
    nextStep(nextTestIter);
  });
}

function addTest(testGen) {
  gTests.push(testGen);
}

var gTests = [];
var gMsgMan;

addTest(function GetState() {
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "GetState",
  });
  let msg = yield waitForTestMsg("State");
  checkMsg(msg, {
    type: "State",
    data: currentStateObj(),
  });
});

addTest(function SetCurrentEngine() {
  let newCurrentEngine = null;
  let oldCurrentEngine = Services.search.currentEngine;
  let engines = Services.search.getVisibleEngines();
  for (let engine of engines) {
    if (engine != oldCurrentEngine) {
      newCurrentEngine = engine;
      break;
    }
  }
  if (!newCurrentEngine) {
    info("Couldn't find a non-selected search engine, " +
         "skipping this part of the test");
    return;
  }
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "SetCurrentEngine",
    data: newCurrentEngine.name,
  });
  Services.obs.addObserver(function obs(subj, topic, data) {
    info("Test observed " + data);
    if (data == "engine-current") {
      ok(true, "Test observed engine-current");
      Services.obs.removeObserver(obs, "browser-search-engine-modified", false);
      nextStep();
    }
  }, "browser-search-engine-modified", false);
  info("Waiting for test to observe engine-current...");
  waitForTestMsg("CurrentEngine");
  let maybeMsg1 = yield null;
  let maybeMsg2 = yield null;
  let msg = maybeMsg1 || maybeMsg2;
  ok(!!msg,
     "Sanity check: One of the yields is for waitForTestMsg and should have " +
     "therefore produced a message object");
  checkMsg(msg, {
    type: "CurrentEngine",
    data: currentEngineObj(newCurrentEngine),
  });

  Services.search.currentEngine = oldCurrentEngine;
  let msg = yield waitForTestMsg("CurrentEngine");
  checkMsg(msg, {
    type: "CurrentEngine",
    data: currentEngineObj(oldCurrentEngine),
  });
});

addTest(function ManageEngines() {
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "ManageEngines",
  });
  let winWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                   getService(Ci.nsIWindowWatcher);
  winWatcher.registerNotification(function onOpen(subj, topic, data) {
    if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
      subj.addEventListener("load", function onLoad() {
        subj.removeEventListener("load", onLoad);
        if (subj.document.documentURI ==
            "chrome://browser/content/search/engineManager.xul") {
          winWatcher.unregisterNotification(onOpen);
          ok(true, "Observed search manager window open");
          is(subj.opener, window,
             "Search engine manager opener should be this chrome window");
          subj.close();
          nextStep();
        }
      });
    }
  });
  info("Waiting for search engine manager window to open...");
  yield null;
});

addTest(function modifyEngine() {
  let engine = Services.search.currentEngine;
  let oldAlias = engine.alias;
  engine.alias = "ContentSearchTest";
  let msg = yield waitForTestMsg("State");
  checkMsg(msg, {
    type: "State",
    data: currentStateObj(),
  });
  engine.alias = oldAlias;
  msg = yield waitForTestMsg("State");
  checkMsg(msg, {
    type: "State",
    data: currentStateObj(),
  });
});

addTest(function search() {
  let engine = Services.search.currentEngine;
  let data = {
    engineName: engine.name,
    searchString: "ContentSearchTest",
    whence: "ContentSearchTest",
  };
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "Search",
    data: data,
  });
  let submissionURL =
    engine.getSubmission(data.searchString, "", data.whence).uri.spec;
  let listener = {
    onStateChange: function (webProg, req, flags, status) {
      let url = req.originalURI.spec;
      info("onStateChange " + url);
      let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                     Ci.nsIWebProgressListener.STATE_START;
      if ((flags & docStart) && webProg.isTopLevel && url == submissionURL) {
        gBrowser.removeProgressListener(listener);
        ok(true, "Search URL loaded");
        req.cancel(Components.results.NS_ERROR_FAILURE);
        nextStep();
      }
    }
  };
  gBrowser.addProgressListener(listener);
  info("Waiting for search URL to load: " + submissionURL);
  yield null;
});

function checkMsg(actualMsg, expectedMsgData) {
  SimpleTest.isDeeply(actualMsg.data, expectedMsgData, "Checking message");
}

function waitForMsg(name, type, callback) {
  info("Waiting for " + name + " message " + type + "...");
  gMsgMan.addMessageListener(name, function onMsg(msg) {
    info("Received " + name + " message " + msg.data.type + "\n");
    if (msg.data.type == type) {
      gMsgMan.removeMessageListener(name, onMsg);
      (callback || nextStep)(msg);
    }
  });
}

function waitForTestMsg(type, callback) {
  waitForMsg(TEST_MSG, type, callback);
}

function addTab(onLoad) {
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", function load() {
    tab.removeEventListener("load", load, true);
    let url = getRootDirectory(gTestPath) + TEST_CONTENT_SCRIPT_BASENAME;
    gMsgMan = tab.linkedBrowser.messageManager;
    gMsgMan.sendAsyncMessage(CONTENT_SEARCH_MSG, {
      type: "AddToWhitelist",
      data: ["about:blank"],
    });
    waitForMsg(CONTENT_SEARCH_MSG, "AddToWhitelistAck", () => {
      gMsgMan.loadFrameScript(url, false);
      onLoad();
    });
  }, true);
  registerCleanupFunction(() => gBrowser.removeTab(tab));
}

function currentStateObj() {
  return {
    engines: Services.search.getVisibleEngines().map(engine => {
      return {
        name: engine.name,
        iconURI: engine.getIconURLBySize(16, 16),
      };
    }),
    currentEngine: currentEngineObj(),
  };
}

function currentEngineObj(expectedCurrentEngine) {
  if (expectedCurrentEngine) {
    is(Services.search.currentEngine.name, expectedCurrentEngine.name,
       "Sanity check: expected current engine");
  }
  return {
    name: Services.search.currentEngine.name,
    logoURI: Services.search.currentEngine.getIconURLBySize(65, 26),
    logo2xURI: Services.search.currentEngine.getIconURLBySize(130, 52),
  };
}
