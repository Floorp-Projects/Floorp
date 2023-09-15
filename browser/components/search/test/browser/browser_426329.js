/* eslint-disable mozilla/no-arbitrary-setTimeout */
ChromeUtils.defineESModuleGetters(this, {
  FormHistoryTestUtils:
    "resource://testing-common/FormHistoryTestUtils.sys.mjs",
});

function expectedURL(aSearchTerms) {
  const ENGINE_HTML_BASE =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/test.html";
  let searchArg = Services.textToSubURI.ConvertAndEscape("utf-8", aSearchTerms);
  return ENGINE_HTML_BASE + "?test=" + searchArg;
}

function simulateClick(aEvent, aTarget) {
  let event = document.createEvent("MouseEvent");
  let ctrlKeyArg = aEvent.ctrlKey || false;
  let altKeyArg = aEvent.altKey || false;
  let shiftKeyArg = aEvent.shiftKey || false;
  let metaKeyArg = aEvent.metaKey || false;
  let buttonArg = aEvent.button || 0;
  event.initMouseEvent(
    "click",
    true,
    true,
    window,
    0,
    0,
    0,
    0,
    0,
    ctrlKeyArg,
    altKeyArg,
    shiftKeyArg,
    metaKeyArg,
    buttonArg,
    null
  );
  aTarget.dispatchEvent(event);
}

// modified from toolkit/components/satchel/test/test_form_autocomplete.html
function checkMenuEntries(expectedValues) {
  let actualValues = getMenuEntries();
  is(
    actualValues.length,
    expectedValues.length,
    "Checking length of expected menu"
  );
  for (let i = 0; i < expectedValues.length; i++) {
    is(actualValues[i], expectedValues[i], "Checking menu entry #" + i);
  }
}

function getMenuEntries() {
  // Could perhaps pull values directly from the controller, but it seems
  // more reliable to test the values that are actually in the richlistbox?
  return Array.from(searchBar.textbox.popup.richlistbox.itemChildren, item =>
    item.getAttribute("ac-value")
  );
}

var searchBar;
var searchButton;
var searchEntries = ["test"];
function promiseSetEngine() {
  return new Promise(resolve => {
    let ss = Services.search;

    function observer(aSub, aTopic, aData) {
      switch (aData) {
        case "engine-added":
          let engine = ss.getEngineByName("Bug 426329");
          ok(engine, "Engine was added.");
          ss.defaultEngine = engine;
          break;
        case "engine-default":
          ok(ss.defaultEngine.name == "Bug 426329", "defaultEngine set");
          searchBar = BrowserSearch.searchBar;
          searchButton = searchBar.querySelector(".search-go-button");
          ok(searchButton, "got search-go-button");

          Services.obs.removeObserver(
            observer,
            "browser-search-engine-modified"
          );
          resolve();
          break;
      }
    }

    Services.obs.addObserver(observer, "browser-search-engine-modified");
    ss.addOpenSearchEngine(
      "http://mochi.test:8888/browser/browser/components/search/test/browser/426329.xml",
      "data:image/x-icon,%00"
    );
  });
}

function promiseRemoveEngine() {
  return new Promise(resolve => {
    let ss = Services.search;

    function observer(aSub, aTopic, aData) {
      if (aData == "engine-removed") {
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        resolve();
      }
    }

    Services.obs.addObserver(observer, "browser-search-engine-modified");
    let engine = ss.getEngineByName("Bug 426329");
    ss.removeEngine(engine);
  });
}

var preSelectedBrowser;
var preTabNo;
async function prepareTest() {
  await Services.search.init();

  preSelectedBrowser = gBrowser.selectedBrowser;
  preTabNo = gBrowser.tabs.length;
  searchBar = BrowserSearch.searchBar;

  await SimpleTest.promiseFocus();

  if (document.activeElement == searchBar) {
    return;
  }

  let focusPromise = BrowserTestUtils.waitForEvent(searchBar.textbox, "focus");
  gURLBar.focus();
  searchBar.focus();
  await focusPromise;
}

add_setup(async function () {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function testSetupEngine() {
  await promiseSetEngine();
  searchBar.value = "test";
});

add_task(async function testReturn() {
  await prepareTest();
  EventUtils.synthesizeKey("KEY_Enter");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(gBrowser.tabs.length, preTabNo, "Return key did not open new tab");
  is(
    gBrowser.currentURI.spec,
    expectedURL(searchBar.value),
    "testReturn opened correct search page"
  );
});

add_task(async function testAltReturn() {
  await prepareTest();
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    EventUtils.synthesizeKey("KEY_Enter", { altKey: true });
  });

  is(gBrowser.tabs.length, preTabNo + 1, "Alt+Return key added new tab");
  is(
    gBrowser.currentURI.spec,
    expectedURL(searchBar.value),
    "testAltReturn opened correct search page"
  );
});

add_task(async function testAltGrReturn() {
  await prepareTest();
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    EventUtils.synthesizeKey("KEY_Enter", { altGraphKey: true });
  });

  is(gBrowser.tabs.length, preTabNo + 1, "AltGr+Return key added new tab");
  is(
    gBrowser.currentURI.spec,
    expectedURL(searchBar.value),
    "testAltGrReturn opened correct search page"
  );
});

// Shift key has no effect for now, so skip it
add_task(async function testShiftAltReturn() {
  /*
  yield* prepareTest();

  let url = expectedURL(searchBar.value);

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url);
  EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true, altKey: true });
  yield newTabPromise;

  is(gBrowser.tabs.length, preTabNo + 1, "Shift+Alt+Return key added new tab");
  is(gBrowser.currentURI.spec, url, "testShiftAltReturn opened correct search page");
  */
});

add_task(async function testLeftClick() {
  await prepareTest();
  simulateClick({ button: 0 }, searchButton);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  is(gBrowser.tabs.length, preTabNo, "LeftClick did not open new tab");
  is(
    gBrowser.currentURI.spec,
    expectedURL(searchBar.value),
    "testLeftClick opened correct search page"
  );
});

add_task(async function testMiddleClick() {
  await prepareTest();
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    simulateClick({ button: 1 }, searchButton);
  });
  is(gBrowser.tabs.length, preTabNo + 1, "MiddleClick added new tab");
  is(
    gBrowser.currentURI.spec,
    expectedURL(searchBar.value),
    "testMiddleClick opened correct search page"
  );
});

add_task(async function testShiftMiddleClick() {
  await prepareTest();

  let url = expectedURL(searchBar.value);

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url);
  simulateClick({ button: 1, shiftKey: true }, searchButton);
  let newTab = await newTabPromise;

  is(gBrowser.tabs.length, preTabNo + 1, "Shift+MiddleClick added new tab");
  is(
    newTab.linkedBrowser.currentURI.spec,
    url,
    "testShiftMiddleClick opened correct search page"
  );
});

add_task(async function testRightClick() {
  preTabNo = gBrowser.tabs.length;
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:blank",
    {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    }
  );
  await new Promise(resolve => {
    setTimeout(function () {
      is(gBrowser.tabs.length, preTabNo, "RightClick did not open new tab");
      is(gBrowser.currentURI.spec, "about:blank", "RightClick did nothing");
      resolve();
    }, 5000);
    simulateClick({ button: 2 }, searchButton);
  });
  // The click in the searchbox focuses it, which opens the suggestion
  // panel. Clean up after ourselves.
  searchBar.textbox.popup.hidePopup();
});

add_task(async function testSearchHistory() {
  let textbox = searchBar._textbox;
  for (let i = 0; i < searchEntries.length; i++) {
    let count = await FormHistoryTestUtils.count(
      textbox.getAttribute("autocompletesearchparam"),
      { value: searchEntries[i], source: "Bug 426329" }
    );
    ok(count > 0, "form history entry '" + searchEntries[i] + "' should exist");
  }
});

add_task(async function testAutocomplete() {
  let popup = searchBar.textbox.popup;
  let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  searchBar.textbox.showHistoryPopup();
  await popupShownPromise;
  checkMenuEntries(searchEntries);
  searchBar.textbox.closePopup();
});

add_task(async function testClearHistory() {
  // Open the textbox context menu to trigger controller attachment.
  let textbox = searchBar.textbox;
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    false,
    event => event.target.classList.contains("textbox-contextmenu")
  );
  EventUtils.synthesizeMouseAtCenter(textbox, {
    type: "contextmenu",
    button: 2,
  });
  await popupShownPromise;
  // Close the context menu.
  let contextMenu = document.querySelector(".textbox-contextmenu");
  contextMenu.hidePopup();

  let menuitem = searchBar._menupopup.querySelector(".searchbar-clear-history");
  ok(!menuitem.disabled, "Clear history menuitem enabled");

  let historyCleared = promiseObserver("satchel-storage-changed");
  searchBar._menupopup.activateItem(menuitem);
  await historyCleared;
  let count = await FormHistoryTestUtils.count(
    textbox.getAttribute("autocompletesearchparam")
  );
  ok(count == 0, "History cleared");
});

add_task(async function asyncCleanup() {
  searchBar.value = "";
  while (gBrowser.tabs.length != 1) {
    gBrowser.removeTab(gBrowser.tabs[0], { animate: false });
  }
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:blank",
    {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    }
  );
  await promiseRemoveEngine();
});

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic, aData) => {
      Services.obs.removeObserver(obs, aTopic);
      resolve(aSubject);
    };
    Services.obs.addObserver(obs, topic);
  });
}
