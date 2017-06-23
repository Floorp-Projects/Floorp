/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const CONTENT_SEARCH_MSG = "ContentSearch";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearch.js";

var gMsgMan;
/* import-globals-from ../../../components/search/test/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/search/test/head.js",
  this);

let originalEngine = Services.search.currentEngine;

add_task(async function setup() {
  await promiseNewEngine("testEngine.xml", {
    setAsCurrent: true,
    testPath: "chrome://mochitests/content/browser/browser/components/search/test/",
  });

  registerCleanupFunction(() => {
    Services.search.currentEngine = originalEngine;
  });
});

add_task(async function GetState() {
  await addTab();
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "GetState",
  });
  let msg = await waitForTestMsg("State");
  checkMsg(msg, {
    type: "State",
    data: await currentStateObj(),
  });
});

add_task(async function SetCurrentEngine() {
  await addTab();
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
  let deferred = PromiseUtils.defer();
  Services.obs.addObserver(function obs(subj, topic, data) {
    info("Test observed " + data);
    if (data == "engine-current") {
      ok(true, "Test observed engine-current");
      Services.obs.removeObserver(obs, "browser-search-engine-modified");
      deferred.resolve();
    }
  }, "browser-search-engine-modified");
  let searchPromise = waitForTestMsg("CurrentEngine");
  info("Waiting for test to observe engine-current...");
  await deferred.promise;
  let msg = await searchPromise;
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await currentEngineObj(newCurrentEngine),
  });

  Services.search.currentEngine = oldCurrentEngine;
  msg = await waitForTestMsg("CurrentEngine");
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await currentEngineObj(oldCurrentEngine),
  });
});

add_task(async function modifyEngine() {
  await addTab();
  let engine = Services.search.currentEngine;
  let oldAlias = engine.alias;
  engine.alias = "ContentSearchTest";
  let msg = await waitForTestMsg("CurrentState");
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(),
  });
  engine.alias = oldAlias;
  msg = await waitForTestMsg("CurrentState");
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(),
  });
});

add_task(async function search() {
  await addTab();
  let engine = Services.search.currentEngine;
  let data = {
    engineName: engine.name,
    searchString: "ContentSearchTest",
    healthReportKey: "ContentSearchTest",
    searchPurpose: "ContentSearchTest",
  };
  let submissionURL =
    engine.getSubmission(data.searchString, "", data.whence).uri.spec;
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "Search",
    data,
    expectedURL: submissionURL,
  });
  let msg = await waitForTestMsg("loadStopped");
  Assert.equal(msg.data.url, submissionURL, "Correct search page loaded");
});

add_task(async function searchInBackgroundTab() {
  // This test is like search(), but it opens a new tab after starting a search
  // in another.  In other words, it performs a search in a background tab.  The
  // search page should be loaded in the same tab that performed the search, in
  // the background tab.
  await addTab();
  let engine = Services.search.currentEngine;
  let data = {
    engineName: engine.name,
    searchString: "ContentSearchTest",
    healthReportKey: "ContentSearchTest",
    searchPurpose: "ContentSearchTest",
  };
  let submissionURL =
    engine.getSubmission(data.searchString, "", data.whence).uri.spec;
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "Search",
    data,
    expectedURL: submissionURL,
  });

  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  registerCleanupFunction(() => gBrowser.removeTab(newTab));

  let msg = await waitForTestMsg("loadStopped");
  Assert.equal(msg.data.url, submissionURL, "Correct search page loaded");
});

add_task(async function badImage() {
  await addTab();
  // If the bad image URI caused an exception to be thrown within ContentSearch,
  // then we'll hang waiting for the CurrentState responses triggered by the new
  // engine.  That's what we're testing, and obviously it shouldn't happen.
  let vals = await waitForNewEngine("contentSearchBadImage.xml", 1);
  let engine = vals[0];
  let finalCurrentStateMsg = vals[vals.length - 1];
  let expectedCurrentState = await currentStateObj();
  let expectedEngine =
    expectedCurrentState.engines.find(e => e.name == engine.name);
  ok(!!expectedEngine, "Sanity check: engine should be in expected state");
  ok(expectedEngine.iconBuffer === null,
     "Sanity check: icon array buffer of engine in expected state " +
     "should be null: " + expectedEngine.iconBuffer);
  checkMsg(finalCurrentStateMsg, {
    type: "CurrentState",
    data: expectedCurrentState,
  });
  // Removing the engine triggers a final CurrentState message.  Wait for it so
  // it doesn't trip up subsequent tests.
  Services.search.removeEngine(engine);
  await waitForTestMsg("CurrentState");
});

add_task(async function GetSuggestions_AddFormHistoryEntry_RemoveFormHistoryEntry() {
  await addTab();

  // Add the test engine that provides suggestions.
  let vals = await waitForNewEngine("contentSearchSuggestions.xml", 0);
  let engine = vals[0];

  let searchStr = "browser_ContentSearch.js-suggestions-";

  // Add a form history suggestion and wait for Satchel to notify about it.
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "AddFormHistoryEntry",
    data: searchStr + "form",
  });
  let deferred = PromiseUtils.defer();
  Services.obs.addObserver(function onAdd(subj, topic, data) {
    if (data == "formhistory-add") {
      Services.obs.removeObserver(onAdd, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed");
  await deferred.promise;

  // Send GetSuggestions using the test engine.  Its suggestions should appear
  // in the remote suggestions in the Suggestions response below.
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "GetSuggestions",
    data: {
      engineName: engine.name,
      searchString: searchStr,
    },
  });

  // Check the Suggestions response.
  let msg = await waitForTestMsg("Suggestions");
  checkMsg(msg, {
    type: "Suggestions",
    data: {
      engineName: engine.name,
      searchString: searchStr,
      formHistory: [searchStr + "form"],
      remote: [searchStr + "foo", searchStr + "bar"],
    },
  });

  // Delete the form history suggestion and wait for Satchel to notify about it.
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "RemoveFormHistoryEntry",
    data: searchStr + "form",
  });
  deferred = PromiseUtils.defer();
  Services.obs.addObserver(function onRemove(subj, topic, data) {
    if (data == "formhistory-remove") {
      Services.obs.removeObserver(onRemove, "satchel-storage-changed");
      executeSoon(() => deferred.resolve());
    }
  }, "satchel-storage-changed");
  await deferred.promise;

  // Send GetSuggestions again.
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "GetSuggestions",
    data: {
      engineName: engine.name,
      searchString: searchStr,
    },
  });

  // The formHistory suggestions in the Suggestions response should be empty.
  msg = await waitForTestMsg("Suggestions");
  checkMsg(msg, {
    type: "Suggestions",
    data: {
      engineName: engine.name,
      searchString: searchStr,
      formHistory: [],
      remote: [searchStr + "foo", searchStr + "bar"],
    },
  });

  // Finally, clean up by removing the test engine.
  Services.search.removeEngine(engine);
  await waitForTestMsg("CurrentState");
});

function buffersEqual(actualArrayBuffer, expectedArrayBuffer) {
  let expectedView = new Int8Array(expectedArrayBuffer);
  let actualView = new Int8Array(actualArrayBuffer);
  for (let i = 0; i < expectedView.length; i++) {
    if (actualView[i] != expectedView[i]) {
      return false;
    }
  }
  return true;
}

function arrayBufferEqual(actualArrayBuffer, expectedArrayBuffer) {
  ok(actualArrayBuffer instanceof ArrayBuffer, "Actual value is ArrayBuffer.");
  ok(expectedArrayBuffer instanceof ArrayBuffer, "Expected value is ArrayBuffer.");
  Assert.equal(actualArrayBuffer.byteLength, expectedArrayBuffer.byteLength,
      "Array buffers have the same length.");
  ok(buffersEqual(actualArrayBuffer, expectedArrayBuffer), "Buffers are equal.");
}

function checkArrayBuffers(actual, expected) {
  if (actual instanceof ArrayBuffer) {
    arrayBufferEqual(actual, expected);
  }
  if (typeof actual == "object") {
    for (let i in actual) {
      checkArrayBuffers(actual[i], expected[i]);
    }
  }
}

function checkMsg(actualMsg, expectedMsgData) {
  let actualMsgData = actualMsg.data;
  SimpleTest.isDeeply(actualMsg.data, expectedMsgData, "Checking message");

  // Engines contain ArrayBuffers which we have to compare byte by byte and
  // not as Objects (like SimpleTest.isDeeply does).
  checkArrayBuffers(actualMsgData, expectedMsgData);
}

function waitForMsg(name, type) {
  return new Promise(resolve => {
    info("Waiting for " + name + " message " + type + "...");
    gMsgMan.addMessageListener(name, function onMsg(msg) {
      info("Received " + name + " message " + msg.data.type + "\n");
      if (msg.data.type == type) {
        gMsgMan.removeMessageListener(name, onMsg);
        resolve(msg);
      }
    });
  });
}

function waitForTestMsg(type) {
  return waitForMsg(TEST_MSG, type);
}

function waitForNewEngine(basename, numImages) {
  info("Waiting for engine to be added: " + basename);

  // Wait for the search events triggered by adding the new engine.
  // engine-added engine-loaded
  let expectedSearchEvents = ["CurrentState", "CurrentState"];
  // engine-changed for each of the images
  for (let i = 0; i < numImages; i++) {
    expectedSearchEvents.push("CurrentState");
  }
  let eventPromises = expectedSearchEvents.map(e => waitForTestMsg(e));

  // Wait for addEngine().
  let addDeferred = PromiseUtils.defer();
  let url = getRootDirectory(gTestPath) + basename;
  Services.search.addEngine(url, null, "", false, {
    onSuccess(engine) {
      info("Search engine added: " + basename);
      addDeferred.resolve(engine);
    },
    onError(errCode) {
      ok(false, "addEngine failed with error code " + errCode);
      addDeferred.reject();
    },
  });

  return Promise.all([addDeferred.promise].concat(eventPromises));
}

function addTab() {
  return new Promise(resolve => {
    let tab = BrowserTestUtils.addTab(gBrowser);
    gBrowser.selectedTab = tab;
    tab.linkedBrowser.addEventListener("load", function() {
      let url = getRootDirectory(gTestPath) + TEST_CONTENT_SCRIPT_BASENAME;
      gMsgMan = tab.linkedBrowser.messageManager;
      gMsgMan.sendAsyncMessage(CONTENT_SEARCH_MSG, {
        type: "AddToWhitelist",
        data: ["about:blank"],
      });
      waitForMsg(CONTENT_SEARCH_MSG, "AddToWhitelistAck").then(() => {
        gMsgMan.loadFrameScript(url, false);
        resolve();
      });
    }, {capture: true, once: true});
    registerCleanupFunction(() => gBrowser.removeTab(tab));
  });
}

var currentStateObj = async function() {
  let state = {
    engines: [],
    currentEngine: await currentEngineObj(),
  };
  for (let engine of Services.search.getVisibleEngines()) {
    let uri = engine.getIconURLBySize(16, 16);
    state.engines.push({
      name: engine.name,
      iconBuffer: await arrayBufferFromDataURI(uri),
      hidden: false,
    });
  }
  return state;
};

var currentEngineObj = async function() {
  let engine = Services.search.currentEngine;
  let uriFavicon = engine.getIconURLBySize(16, 16);
  let bundle = Services.strings.createBundle("chrome://global/locale/autocomplete.properties");
  return {
    name: engine.name,
    placeholder: bundle.formatStringFromName("searchWithEngine", [engine.name], 1),
    iconBuffer: await arrayBufferFromDataURI(uriFavicon),
  };
};

function arrayBufferFromDataURI(uri) {
  if (!uri) {
    return Promise.resolve(null);
  }
  return new Promise(resolve => {
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", uri, true);
    xhr.responseType = "arraybuffer";
    xhr.onerror = () => {
      resolve(null);
    };
    xhr.onload = () => {
      resolve(xhr.response);
    };
    try {
      xhr.send();
    } catch (err) {
      resolve(null);
    }
  });
}
