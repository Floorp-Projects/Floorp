/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
});

SearchTestUtils.init(this);

const SERVICE_EVENT_TYPE = "ContentSearchService";
const CLIENT_EVENT_TYPE = "ContentSearchClient";

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

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtab.preload", false],
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
    ],
  });

  await SearchTestUtils.promiseNewSearchEngine({
    url: "chrome://mochitests/content/browser/browser/components/search/test/browser/testEngine.xml",
    setAsDefault: true,
  });

  await SearchTestUtils.promiseNewSearchEngine({
    url: "chrome://mochitests/content/browser/browser/components/search/test/browser/testEngine_diacritics.xml",
    setAsDefaultPrivate: true,
  });

  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "testEngine_chromeicon.xml",
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
  await Services.search.setDefault(
    oldDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  msg = await enginePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentEngine",
    data: await constructEngineObj(oldDefaultEngine),
  });
});

// ContentSearchChild doesn't support setting the private engine at this time
// as it doesn't need to, so we just test updating the default here.
add_task(async function setDefaultEnginePrivate() {
  const engine = await Services.search.getEngineByName("FooChromeIcon");
  const { browser } = await addTab();
  let enginePromise = await waitForTestMsg(browser, "CurrentPrivateEngine");
  await Services.search.setDefaultPrivate(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
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
  engine.hideOneOffButton = true;
  let msg = await statePromise.donePromise;
  checkMsg(msg, {
    type: "CurrentState",
    data: await currentStateObj(undefined, "Foo \u2661"),
  });
  statePromise = await waitForTestMsg(browser, "CurrentState");
  engine.hideOneOffButton = false;
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
  let [engine, currentStateMsg] = await waitForNewEngine(
    browser,
    "contentSearchBadImage.xml"
  );
  let expectedCurrentState = await currentStateObj();
  let expectedEngine = expectedCurrentState.engines.find(
    e => e.name == engine.name
  );
  ok(!!expectedEngine, "Sanity check: engine should be in expected state");
  ok(
    expectedEngine.iconData ===
      "chrome://browser/skin/search-engine-placeholder.png",
    "Sanity check: icon of engine in expected state should be the placeholder: " +
      expectedEngine.iconData
  );
  checkMsg(currentStateMsg, {
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
    let [engine] = await waitForNewEngine(
      browser,
      "contentSearchSuggestions.xml"
    );

    let searchStr = "browser_contentSearch.js-suggestions-";

    // Add a form history suggestion and wait for Satchel to notify about it.
    sendEventToContent(browser, {
      type: "AddFormHistoryEntry",
      data: {
        value: searchStr + "form",
        engineName: engine.name,
      },
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

async function waitForTestMsg(browser, type) {
  // We call SpecialPowers.spawn twice because we must let the first one
  // complete so that the listener is added before we return from this function.
  // In the second one, we wait for the signal that the expected message has
  // been received.
  await SpecialPowers.spawn(
    browser,
    [SERVICE_EVENT_TYPE, type],
    async (childEvent, childType) => {
      function listener(event) {
        if (event.detail.type != childType) {
          return;
        }

        content.eventDetails = event.detail;
        content.removeEventListener(childEvent, listener, true);
      }
      // Ensure any previous details are cleared, so that we don't
      // get the wrong ones by mistake.
      content.eventDetails = undefined;
      content.addEventListener(childEvent, listener, true);
    }
  );

  let donePromise = SpecialPowers.spawn(browser, [type], async childType => {
    await ContentTaskUtils.waitForCondition(() => {
      return !!content.eventDetails;
    }, "Expected " + childType + " event");
    return content.eventDetails;
  });

  return { donePromise };
}

async function waitForNewEngine(browser, basename) {
  info("Waiting for engine to be added: " + basename);

  // Wait for the search events triggered by adding the new engine.
  // There are two events triggerd by engine-added and engine-loaded
  let statePromise = await waitForTestMsg(browser, "CurrentState");

  let engine = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + basename,
  });
  return [engine, await statePromise.donePromise];
}

async function addTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab"
  );
  registerCleanupFunction(() => gBrowser.removeTab(tab));

  return { browser: tab.linkedBrowser };
}

var currentStateObj = async function (isPrivateWindowValue, hiddenEngine = "") {
  let state = {
    engines: [],
    currentEngine: await constructEngineObj(await Services.search.getDefault()),
    currentPrivateEngine: await constructEngineObj(
      await Services.search.getDefaultPrivate()
    ),
  };
  for (let engine of await Services.search.getVisibleEngines()) {
    let uri = engine.getIconURL(16);
    state.engines.push({
      name: engine.name,
      iconData: await iconDataFromURI(uri),
      hidden: engine.name == hiddenEngine,
      isAppProvided: engine.isAppProvided,
    });
  }
  if (typeof isPrivateWindowValue == "boolean") {
    state.isInPrivateBrowsingMode = isPrivateWindowValue;
    state.isAboutPrivateBrowsing = isPrivateWindowValue;
  }
  return state;
};

async function constructEngineObj(engine) {
  let uriFavicon = engine.getIconURL(16);
  return {
    name: engine.name,
    iconData: await iconDataFromURI(uriFavicon),
    isAppProvided: engine.isAppProvided,
  };
}

function iconDataFromURI(uri) {
  if (!uri) {
    return Promise.resolve(
      "chrome://browser/skin/search-engine-placeholder.png"
    );
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
      resolve("chrome://browser/skin/search-engine-placeholder.png");
    };
    xhr.onload = () => {
      arrayBufferIconTested = true;
      resolve(xhr.response);
    };
    try {
      xhr.send();
    } catch (err) {
      resolve("chrome://browser/skin/search-engine-placeholder.png");
    }
  });
}
