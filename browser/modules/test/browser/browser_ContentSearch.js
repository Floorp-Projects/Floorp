/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const CONTENT_SEARCH_MSG = "ContentSearch";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearch.js";

/* import-globals-from ../../../components/search/test/browser/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/search/test/browser/head.js",
  this);

var originalEngine;

var arrayBufferIconTested = false;
var plainURIIconTested = false;

add_task(async function setup() {
  originalEngine = await Services.search.getDefault();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtab.preload", false],
    ],
  });

  await promiseNewEngine("testEngine.xml", {
    setAsCurrent: true,
    testPath: "chrome://mochitests/content/browser/browser/components/search/test/browser/",
  });

  await promiseNewEngine("testEngine_chromeicon.xml", {
    setAsCurrent: false,
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(originalEngine);
  });
});

add_task(async function GetState() {
  let { mm } = await addTab();
  mm.sendAsyncMessage(TEST_MSG, {
    type: "GetState",
  });
  let msg = await waitForTestMsg(mm, "State");
  checkMsg(msg, {
    type: "State",
    data: await currentStateObj(),
  });

  ok(arrayBufferIconTested, "ArrayBuffer path for the iconData was tested");
  ok(plainURIIconTested, "Plain URI path for the iconData was tested");
});

add_task(async function SetDefaultEngine() {
  let { mm } = await addTab();
  let newDefaultEngine = null;
  let oldDefaultEngine = await Services.search.getDefault();
  let engines = await Services.search.getVisibleEngines();
  for (let engine of engines) {
    if (engine != oldDefaultEngine) {
      newDefaultEngine = engine;
      break;
    }
  }
  if (!newDefaultEngine) {
    info("Couldn't find a non-selected search engine, " +
         "skipping this part of the test");
    return;
  }
  mm.sendAsyncMessage(TEST_MSG, {
    type: "SetCurrentEngine",
    data: newDefaultEngine.name,
  });
  let deferred = PromiseUtils.defer();
  Services.obs.addObserver(function obs(subj, topic, data) {
    info("Test observed " + data);
    if (data == "engine-default") {
      ok(true, "Test observed engine-default");
      Services.obs.removeObserver(obs, "browser-search-engine-modified");
      deferred.resolve();
    }
  }, "browser-search-engine-modified");
  let searchPromise = waitForTestMsg(mm, "CurrentEngine");
  info("Waiting for test to observe engine-default...");
  await deferred.promise;
  let msg = await searchPromise;
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await defaultEngineObj(newDefaultEngine),
  });

  await Services.search.setDefault(oldDefaultEngine);
  msg = await waitForTestMsg(mm, "CurrentEngine");
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await defaultEngineObj(oldDefaultEngine),
  });
});

add_task(async function modifyEngine() {
  let { mm } = await addTab();
  let engine = await Services.search.getDefault();
  let oldAlias = engine.alias;
  engine.alias = "ContentSearchTest";
  let msg = await waitForTestMsg(mm, "CurrentState");
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(),
  });
  engine.alias = oldAlias;
  msg = await waitForTestMsg(mm, "CurrentState");
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(),
  });
});

add_task(async function search() {
  let { browser } = await addTab();
  let engine = await Services.search.getDefault();
  let data = {
    engineName: engine.name,
    searchString: "ContentSearchTest",
    healthReportKey: "ContentSearchTest",
    searchPurpose: "ContentSearchTest",
  };
  let submissionURL =
    engine.getSubmission(data.searchString, "", data.whence).uri.spec;

  await performSearch(browser, data, submissionURL);
});

add_task(async function searchInBackgroundTab() {
  // This test is like search(), but it opens a new tab after starting a search
  // in another.  In other words, it performs a search in a background tab.  The
  // search page should be loaded in the same tab that performed the search, in
  // the background tab.
  let { browser } = await addTab();
  let engine = await Services.search.getDefault();
  let data = {
    engineName: engine.name,
    searchString: "ContentSearchTest",
    healthReportKey: "ContentSearchTest",
    searchPurpose: "ContentSearchTest",
  };
  let submissionURL =
    engine.getSubmission(data.searchString, "", data.whence).uri.spec;

  let searchPromise = performSearch(browser, data, submissionURL);
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  registerCleanupFunction(() => gBrowser.removeTab(newTab));

  await searchPromise;
});

add_task(async function badImage() {
  let { mm } = await addTab();
  // If the bad image URI caused an exception to be thrown within ContentSearch,
  // then we'll hang waiting for the CurrentState responses triggered by the new
  // engine.  That's what we're testing, and obviously it shouldn't happen.
  let vals = await waitForNewEngine(mm, "contentSearchBadImage.xml", 1);
  let engine = vals[0];
  let finalCurrentStateMsg = vals[vals.length - 1];
  let expectedCurrentState = await currentStateObj();
  let expectedEngine =
    expectedCurrentState.engines.find(e => e.name == engine.name);
  ok(!!expectedEngine, "Sanity check: engine should be in expected state");
  ok(expectedEngine.iconData === null,
     "Sanity check: icon array buffer of engine in expected state " +
     "should be null: " + expectedEngine.iconData);
  checkMsg(finalCurrentStateMsg, {
    type: "CurrentState",
    data: expectedCurrentState,
  });
  // Removing the engine triggers a final CurrentState message.  Wait for it so
  // it doesn't trip up subsequent tests.
  await Services.search.removeEngine(engine);
  await waitForTestMsg(mm, "CurrentState");
});

add_task(async function GetSuggestions_AddFormHistoryEntry_RemoveFormHistoryEntry() {
  let { mm } = await addTab();

  // Add the test engine that provides suggestions.
  let vals = await waitForNewEngine(mm, "contentSearchSuggestions.xml", 0);
  let engine = vals[0];

  let searchStr = "browser_ContentSearch.js-suggestions-";

  // Add a form history suggestion and wait for Satchel to notify about it.
  mm.sendAsyncMessage(TEST_MSG, {
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
  mm.sendAsyncMessage(TEST_MSG, {
    type: "GetSuggestions",
    data: {
      engineName: engine.name,
      searchString: searchStr,
    },
  });

  // Check the Suggestions response.
  let msg = await waitForTestMsg(mm, "Suggestions");
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
  mm.sendAsyncMessage(TEST_MSG, {
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
  mm.sendAsyncMessage(TEST_MSG, {
    type: "GetSuggestions",
    data: {
      engineName: engine.name,
      searchString: searchStr,
    },
  });

  // The formHistory suggestions in the Suggestions response should be empty.
  msg = await waitForTestMsg(mm, "Suggestions");
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
  await Services.search.removeEngine(engine);
  await waitForTestMsg(mm, "CurrentState");
});

async function performSearch(browser, data, expectedURL) {
  let mm = browser.messageManager;
  let stoppedPromise = BrowserTestUtils.browserStopped(browser, expectedURL);
  mm.sendAsyncMessage(TEST_MSG, {
    type: "Search",
    data,
    expectedURL,
  });

  await stoppedPromise;
  // BrowserTestUtils.browserStopped should ensure this, but let's
  // be absolutely sure.
  Assert.equal(browser.currentURI.spec, expectedURL, "Correct search page loaded");
}

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

function waitForTestMsg(mm, type) {
  return new Promise(resolve => {
    info("Waiting for " + TEST_MSG + " message " + type + "...");
    mm.addMessageListener(TEST_MSG, function onMsg(msg) {
      info("Received " + TEST_MSG + " message " + msg.data.type + "\n");
      if (msg.data.type == type) {
        mm.removeMessageListener(TEST_MSG, onMsg);
        resolve(msg);
      }
    });
  });
}

function waitForNewEngine(mm, basename, numImages) {
  info("Waiting for engine to be added: " + basename);

  // Wait for the search events triggered by adding the new engine.
  // engine-added engine-loaded
  let expectedSearchEvents = ["CurrentState", "CurrentState"];
  // engine-changed for each of the images
  for (let i = 0; i < numImages; i++) {
    expectedSearchEvents.push("CurrentState");
  }
  let eventPromises = expectedSearchEvents.map(e => waitForTestMsg(mm, e));

  // Wait for addEngine().
  let url = getRootDirectory(gTestPath) + basename;
  return Promise.all([Services.search.addEngine(url, "", false)].concat(eventPromises));
}

async function addTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:newtab");
  registerCleanupFunction(() => gBrowser.removeTab(tab));

  let url = getRootDirectory(gTestPath) + TEST_CONTENT_SCRIPT_BASENAME;
  let mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript(url, false);
  return { browser: tab.linkedBrowser, mm };
}

var currentStateObj = async function() {
  let state = {
    engines: [],
    currentEngine: await defaultEngineObj(),
  };
  for (let engine of await Services.search.getVisibleEngines()) {
    let uri = engine.getIconURLBySize(16, 16);
    state.engines.push({
      name: engine.name,
      iconData: await iconDataFromURI(uri),
      hidden: false,
      identifier: engine.identifier,
    });
  }
  return state;
};

var defaultEngineObj = async function() {
  let engine = await Services.search.getDefault();
  let uriFavicon = engine.getIconURLBySize(16, 16);
  let bundle = Services.strings.createBundle("chrome://global/locale/autocomplete.properties");
  return {
    name: engine.name,
    placeholder: bundle.formatStringFromName("searchWithEngine", [engine.name], 1),
    iconData: await iconDataFromURI(uriFavicon),
  };
};

function iconDataFromURI(uri) {
  if (!uri) {
    return Promise.resolve(null);
  }

  if (!uri.startsWith("data:")) {
    plainURIIconTested = true;
    return Promise.resolve(uri);
  }

  return new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", uri, true);
    xhr.responseType = "arraybuffer";
    xhr.onerror = () => {
      resolve(null);
    };
    xhr.onload = () => {
      arrayBufferIconTested = true;
      resolve(xhr.response);
    };
    try {
      xhr.send();
    } catch (err) {
      resolve(null);
    }
  });
}
