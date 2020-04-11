/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SERVICE_EVENT_TYPE = "ContentSearchService";
const CLIENT_EVENT_TYPE = "ContentSearchClient";

/* import-globals-from ../../../components/search/test/browser/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/search/test/browser/head.js",
  this
);

var arrayBufferIconTested = false;
var plainURIIconTested = false;

function sendEventToContent(browser, data) {
  return SpecialPowers.spawn(
    browser,
    [CLIENT_EVENT_TYPE, data],
    (eventName, eventData) => {
      content.dispatchEvent(
        new content.CustomEvent(eventName, {
          detail: Cu.cloneInto(eventData, content),
        })
      );
    }
  );
}

add_task(async function setup() {
  const originalEngine = await Services.search.getDefault();
  const originalPrivateEngine = await Services.search.getDefaultPrivate();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtab.preload", false],
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
    ],
  });

  await promiseNewEngine("testEngine.xml", {
    setAsCurrent: true,
    testPath:
      "chrome://mochitests/content/browser/browser/components/search/test/browser/",
  });

  await promiseNewEngine("testEngine_diacritics.xml", {
    setAsCurrent: false,
    setAsCurrentPrivate: true,
    testPath:
      "chrome://mochitests/content/browser/browser/components/search/test/browser/",
  });

  await promiseNewEngine("testEngine_chromeicon.xml", {
    setAsCurrent: false,
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(originalEngine);
    await Services.search.setDefaultPrivate(originalPrivateEngine);
  });
});

add_task(async function GetState() {
  let { browser } = await addTab();
  let statePromise = await waitForTestMsg(browser, "State");
  sendEventToContent(browser, {
    type: "GetState",
  });
  let msg = await statePromise.donePromise;
  checkMsg(msg, {
    type: "State",
    data: await currentStateObj(false),
  });

  ok(arrayBufferIconTested, "ArrayBuffer path for the iconData was tested");
  ok(plainURIIconTested, "Plain URI path for the iconData was tested");
});

add_task(async function SetDefaultEngine() {
  let { browser } = await addTab();
  let newDefaultEngine = await Services.search.getEngineByName("FooChromeIcon");
  let oldDefaultEngine = await Services.search.getDefault();
  let searchPromise = await waitForTestMsg(browser, "CurrentEngine");
  sendEventToContent(browser, {
    type: "SetCurrentEngine",
    data: newDefaultEngine.name,
  });
  let deferredPromise = new Promise(resolve => {
    Services.obs.addObserver(function obs(subj, topic, data) {
      info("Test observed " + data);
      if (data == "engine-default") {
        ok(true, "Test observed engine-default");
        Services.obs.removeObserver(obs, "browser-search-engine-modified");
        resolve();
      }
    }, "browser-search-engine-modified");
  });
  info("Waiting for test to observe engine-default...");
  await deferredPromise;
  let msg = await searchPromise.donePromise;
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await constructEngineObj(newDefaultEngine),
  });

  let enginePromise = await waitForTestMsg(browser, "CurrentEngine");
  await Services.search.setDefault(oldDefaultEngine);
  msg = await enginePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await constructEngineObj(oldDefaultEngine),
  });
});

// ContentSearch.jsm doesn't support setting the private engine at this time
// as it doesn't need to, so we just test updating the default here.
add_task(async function setDefaultEnginePrivate() {
  const engine = await Services.search.getEngineByName("FooChromeIcon");
  const { browser } = await addTab();
  let enginePromise = await waitForTestMsg(browser, "CurrentPrivateEngine");
  await Services.search.setDefaultPrivate(engine);
  let msg = await enginePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentPrivateEngine",
    data: await constructEngineObj(engine),
  });
});

add_task(async function modifyEngine() {
  let { browser } = await addTab();
  let engine = await Services.search.getDefault();
  let oldAlias = engine.alias;
  let statePromise = await waitForTestMsg(browser, "CurrentState");
  engine.alias = "ContentSearchTest";
  let msg = await statePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(),
  });
  statePromise = await waitForTestMsg(browser, "CurrentState");
  engine.alias = oldAlias;
  msg = await statePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(),
  });
});

add_task(async function test_hideEngine() {
  let { browser } = await addTab();
  let engine = await Services.search.getEngineByName("Foo \u2661");
  let statePromise = await waitForTestMsg(browser, "CurrentState");
  Services.prefs.setStringPref("browser.search.hiddenOneOffs", engine.name);
  let msg = await statePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(undefined, "Foo \u2661"),
  });
  statePromise = await waitForTestMsg(browser, "CurrentState");
  Services.prefs.clearUserPref("browser.search.hiddenOneOffs");
  msg = await statePromise.donePromise;
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
  let submissionURL = engine.getSubmission(data.searchString, "", data.whence)
    .uri.spec;

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
  let submissionURL = engine.getSubmission(data.searchString, "", data.whence)
    .uri.spec;

  let searchPromise = performSearch(browser, data, submissionURL);
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  registerCleanupFunction(() => gBrowser.removeTab(newTab));

  await searchPromise;
});

add_task(async function badImage() {
  let { browser } = await addTab();
  // If the bad image URI caused an exception to be thrown within ContentSearch,
  // then we'll hang waiting for the CurrentState responses triggered by the new
  // engine.  That's what we're testing, and obviously it shouldn't happen.
  let vals = await waitForNewEngine(browser, "contentSearchBadImage.xml", 1);
  let engine = vals[0];
  let finalCurrentStateMsg = vals[vals.length - 1];
  let expectedCurrentState = await currentStateObj();
  let expectedEngine = expectedCurrentState.engines.find(
    e => e.name == engine.name
  );
  ok(!!expectedEngine, "Sanity check: engine should be in expected state");
  ok(
    expectedEngine.iconData === null,
    "Sanity check: icon array buffer of engine in expected state " +
      "should be null: " +
      expectedEngine.iconData
  );
  checkMsg(finalCurrentStateMsg, {
    type: "CurrentState",
    data: expectedCurrentState,
  });
  // Removing the engine triggers a final CurrentState message.  Wait for it so
  // it doesn't trip up subsequent tests.
  let statePromise = await waitForTestMsg(browser, "CurrentState");
  await Services.search.removeEngine(engine);
  await statePromise.donePromise;
});

add_task(
  async function GetSuggestions_AddFormHistoryEntry_RemoveFormHistoryEntry() {
    let { browser } = await addTab();

    // Add the test engine that provides suggestions.
    let vals = await waitForNewEngine(
      browser,
      "contentSearchSuggestions.xml",
      0
    );
    let engine = vals[0];

    let searchStr = "browser_ContentSearch.js-suggestions-";

    // Add a form history suggestion and wait for Satchel to notify about it.
    sendEventToContent(browser, {
      type: "AddFormHistoryEntry",
      data: searchStr + "form",
    });
    await new Promise(resolve => {
      Services.obs.addObserver(function onAdd(subj, topic, data) {
        if (data == "formhistory-add") {
          Services.obs.removeObserver(onAdd, "satchel-storage-changed");
          executeSoon(resolve);
        }
      }, "satchel-storage-changed");
    });

    // Send GetSuggestions using the test engine.  Its suggestions should appear
    // in the remote suggestions in the Suggestions response below.
    let suggestionsPromise = await waitForTestMsg(browser, "Suggestions");
    sendEventToContent(browser, {
      type: "GetSuggestions",
      data: {
        engineName: engine.name,
        searchString: searchStr,
      },
    });

    // Check the Suggestions response.
    let msg = await suggestionsPromise.donePromise;
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
    sendEventToContent(browser, {
      type: "RemoveFormHistoryEntry",
      data: searchStr + "form",
    });

    await new Promise(resolve => {
      Services.obs.addObserver(function onRemove(subj, topic, data) {
        if (data == "formhistory-remove") {
          Services.obs.removeObserver(onRemove, "satchel-storage-changed");
          executeSoon(resolve);
        }
      }, "satchel-storage-changed");
    });

    // Send GetSuggestions again.
    suggestionsPromise = await waitForTestMsg(browser, "Suggestions");
    sendEventToContent(browser, {
      type: "GetSuggestions",
      data: {
        engineName: engine.name,
        searchString: searchStr,
      },
    });

    // The formHistory suggestions in the Suggestions response should be empty.
    msg = await suggestionsPromise.donePromise;
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
    let statePromise = await waitForTestMsg(browser, "CurrentState");
    await Services.search.removeEngine(engine);
    await statePromise.donePromise;
  }
);

async function performSearch(browser, data, expectedURL) {
  let stoppedPromise = BrowserTestUtils.browserStopped(browser, expectedURL);
  sendEventToContent(browser, {
    type: "Search",
    data,
    expectedURL,
  });

  await stoppedPromise;
  // BrowserTestUtils.browserStopped should ensure this, but let's
  // be absolutely sure.
  Assert.equal(
    browser.currentURI.spec,
    expectedURL,
    "Correct search page loaded"
  );
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
  ok(
    expectedArrayBuffer instanceof ArrayBuffer,
    "Expected value is ArrayBuffer."
  );
  Assert.equal(
    actualArrayBuffer.byteLength,
    expectedArrayBuffer.byteLength,
    "Array buffers have the same length."
  );
  ok(
    buffersEqual(actualArrayBuffer, expectedArrayBuffer),
    "Buffers are equal."
  );
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
  SimpleTest.isDeeply(actualMsg, expectedMsgData, "Checking message");

  // Engines contain ArrayBuffers which we have to compare byte by byte and
  // not as Objects (like SimpleTest.isDeeply does).
  checkArrayBuffers(actualMsg, expectedMsgData);
}

async function waitForTestMsg(browser, type, count = 1) {
  await SpecialPowers.spawn(
    browser,
    [SERVICE_EVENT_TYPE, type, count],
    (childEvent, childType, childCount) => {
      content.eventDetails = [];
      function listener(event) {
        if (event.detail.type != childType) {
          return;
        }

        content.eventDetails.push(event.detail);

        if (--childCount > 0) {
          return;
        }

        content.removeEventListener(childEvent, listener, true);
      }
      content.addEventListener(childEvent, listener, true);
    }
  );

  let donePromise = SpecialPowers.spawn(
    browser,
    [type, count],
    async (childType, childCount) => {
      await ContentTaskUtils.waitForCondition(() => {
        return content.eventDetails.length == childCount;
      }, "Expected " + childType + " event");

      return childCount > 1 ? content.eventDetails : content.eventDetails[0];
    }
  );

  return { donePromise };
}

async function waitForNewEngine(browser, basename, numImages) {
  info("Waiting for engine to be added: " + basename);

  // Wait for the search events triggered by adding the new engine.
  // engine-added engine-loaded
  let count = 2;
  // engine-changed for each of the images
  for (let i = 0; i < numImages; i++) {
    count++;
  }

  let statePromise = await waitForTestMsg(browser, "CurrentState", count);

  // Wait for addEngine().
  let url = getRootDirectory(gTestPath) + basename;
  let engine = await Services.search.addEngine(url, "", false);
  let results = await statePromise.donePromise;
  return [engine, ...results];
}

async function addTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab"
  );
  registerCleanupFunction(() => gBrowser.removeTab(tab));

  return { browser: tab.linkedBrowser };
}

var currentStateObj = async function(isPrivateWindowValue, hiddenEngine = "") {
  let state = {
    engines: [],
    currentEngine: await constructEngineObj(await Services.search.getDefault()),
    currentPrivateEngine: await constructEngineObj(
      await Services.search.getDefaultPrivate()
    ),
  };
  for (let engine of await Services.search.getVisibleEngines()) {
    let uri = engine.getIconURLBySize(16, 16);
    state.engines.push({
      name: engine.name,
      iconData: await iconDataFromURI(uri),
      hidden: engine.name == hiddenEngine,
      identifier: engine.identifier,
    });
  }
  if (typeof isPrivateWindowValue == "boolean") {
    state.isPrivateWindow = isPrivateWindowValue;
  }
  return state;
};

async function constructEngineObj(engine) {
  let uriFavicon = engine.getIconURLBySize(16, 16);
  let bundle = Services.strings.createBundle(
    "chrome://global/locale/autocomplete.properties"
  );
  return {
    name: engine.name,
    placeholder: bundle.formatStringFromName("searchWithEngine", [engine.name]),
    iconData: await iconDataFromURI(uriFavicon),
  };
}

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
