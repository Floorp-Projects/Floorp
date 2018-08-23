/* eslint-disable mozilla/no-arbitrary-setTimeout */
ChromeUtils.defineModuleGetter(this, "FormHistory",
  "resource://gre/modules/FormHistory.jsm");

function expectedURL(aSearchTerms) {
  const ENGINE_HTML_BASE = "http://mochi.test:8888/browser/browser/components/search/test/test.html";
  var searchArg = Services.textToSubURI.ConvertAndEscape("utf-8", aSearchTerms);
  return ENGINE_HTML_BASE + "?test=" + searchArg;
}

function simulateClick(aEvent, aTarget) {
  var event = document.createEvent("MouseEvent");
  var ctrlKeyArg  = aEvent.ctrlKey || false;
  var altKeyArg   = aEvent.altKey || false;
  var shiftKeyArg = aEvent.shiftKey || false;
  var metaKeyArg  = aEvent.metaKey || false;
  var buttonArg   = aEvent.button || 0;
  event.initMouseEvent("click", true, true, window,
                        0, 0, 0, 0, 0,
                        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                        buttonArg, null);
  aTarget.dispatchEvent(event);
}

// modified from toolkit/components/satchel/test/test_form_autocomplete.html
function checkMenuEntries(expectedValues) {
  var actualValues = getMenuEntries();
  is(actualValues.length, expectedValues.length, "Checking length of expected menu");
  for (var i = 0; i < expectedValues.length; i++)
    is(actualValues[i], expectedValues[i], "Checking menu entry #" + i);
}

function getMenuEntries() {
  // Could perhaps pull values directly from the controller, but it seems
  // more reliable to test the values that are actually in the richlistbox?
  return Array.map(searchBar.textbox.popup.richlistbox.children,
                   item => item.getAttribute("ac-value"));
}

function countEntries(name, value) {
  return new Promise(resolve => {
    let count = 0;
    let obj = name && value ? {fieldname: name, value} : {};
    FormHistory.count(obj,
                      { handleResult(result) { count = result; },
                        handleError(error) { throw error; },
                        handleCompletion(reason) {
                          if (!reason) {
                            resolve(count);
                          }
                        }
                      });
  });
}

var searchBar;
var searchButton;
var searchEntries = ["test"];
function promiseSetEngine() {
  return new Promise(resolve => {
    var ss = Services.search;

    function observer(aSub, aTopic, aData) {
      switch (aData) {
        case "engine-added":
          var engine = ss.getEngineByName("Bug 426329");
          ok(engine, "Engine was added.");
          ss.currentEngine = engine;
          break;
        case "engine-current":
          ok(ss.currentEngine.name == "Bug 426329", "currentEngine set");
          searchBar = BrowserSearch.searchBar;
          searchButton = document.getAnonymousElementByAttribute(searchBar,
                             "anonid", "search-go-button");
          ok(searchButton, "got search-go-button");
          searchBar.value = "test";

          Services.obs.removeObserver(observer, "browser-search-engine-modified");
          resolve();
          break;
      }
    }

    Services.obs.addObserver(observer, "browser-search-engine-modified");
    ss.addEngine("http://mochi.test:8888/browser/browser/components/search/test/426329.xml",
                 null, "data:image/x-icon,%00", false);
  });
}

function promiseRemoveEngine() {
  return new Promise(resolve => {
    var ss = Services.search;

    function observer(aSub, aTopic, aData) {
      if (aData == "engine-removed") {
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        resolve();
      }
    }

    Services.obs.addObserver(observer, "browser-search-engine-modified");
    var engine = ss.getEngineByName("Bug 426329");
    ss.removeEngine(engine);
  });
}


var preSelectedBrowser;
var preTabNo;
async function prepareTest() {
  preSelectedBrowser = gBrowser.selectedBrowser;
  preTabNo = gBrowser.tabs.length;
  searchBar = BrowserSearch.searchBar;

  await SimpleTest.promiseFocus();

  if (document.activeElement == searchBar)
    return;

  let focusPromise = BrowserTestUtils.waitForEvent(searchBar, "focus");
  gURLBar.focus();
  searchBar.focus();
  await focusPromise;
}

add_task(async function testSetup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function testSetupEngine() {
  await promiseSetEngine();
});

add_task(async function testReturn() {
  await prepareTest();
  EventUtils.synthesizeKey("KEY_Enter");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(gBrowser.tabs.length, preTabNo, "Return key did not open new tab");
  is(gBrowser.currentURI.spec, expectedURL(searchBar.value), "testReturn opened correct search page");
});

add_task(async function testAltReturn() {
  await prepareTest();
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    EventUtils.synthesizeKey("KEY_Enter", {altKey: true});
  });

  is(gBrowser.tabs.length, preTabNo + 1, "Alt+Return key added new tab");
  is(gBrowser.currentURI.spec, expectedURL(searchBar.value), "testAltReturn opened correct search page");
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
  is(gBrowser.currentURI.spec, expectedURL(searchBar.value), "testLeftClick opened correct search page");
});

add_task(async function testMiddleClick() {
  await prepareTest();
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    simulateClick({ button: 1 }, searchButton);
  });
  is(gBrowser.tabs.length, preTabNo + 1, "MiddleClick added new tab");
  is(gBrowser.currentURI.spec, expectedURL(searchBar.value), "testMiddleClick opened correct search page");
});

add_task(async function testShiftMiddleClick() {
  await prepareTest();

  let url = expectedURL(searchBar.value);

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url);
  simulateClick({ button: 1, shiftKey: true }, searchButton);
  let newTab = await newTabPromise;

  is(gBrowser.tabs.length, preTabNo + 1, "Shift+MiddleClick added new tab");
  is(newTab.linkedBrowser.currentURI.spec, url, "testShiftMiddleClick opened correct search page");
});

add_task(async function testRightClick() {
  preTabNo = gBrowser.tabs.length;
  gBrowser.selectedBrowser.loadURI("about:blank");
  await new Promise(resolve => {
    setTimeout(function() {
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
  var textbox = searchBar._textbox;
  for (var i = 0; i < searchEntries.length; i++) {
    let count = await countEntries(textbox.getAttribute("autocompletesearchparam"), searchEntries[i]);
    ok(count > 0, "form history entry '" + searchEntries[i] + "' should exist");
  }
});

add_task(async function testAutocomplete() {
  var popup = searchBar.textbox.popup;
  let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  searchBar.textbox.showHistoryPopup();
  await popupShownPromise;
  checkMenuEntries(searchEntries);
});

add_task(async function testClearHistory() {
  // Open the textbox context menu to trigger controller attachment.
  let textbox = searchBar.textbox;
  let popupShownPromise = BrowserTestUtils.waitForEvent(textbox, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, { type: "contextmenu", button: 2 });
  await popupShownPromise;
  // Close the context menu.
  EventUtils.synthesizeKey("KEY_Escape");

  let controller = searchBar.textbox.controllers.getControllerForCommand("cmd_clearhistory");
  ok(controller.isCommandEnabled("cmd_clearhistory"), "Clear history command enabled");

  let historyCleared = promiseObserver("satchel-storage-changed");
  controller.doCommand("cmd_clearhistory");
  await historyCleared;
  let count = await countEntries();
  ok(count == 0, "History cleared");
});

add_task(async function asyncCleanup() {
  searchBar.value = "";
  while (gBrowser.tabs.length != 1) {
    gBrowser.removeTab(gBrowser.tabs[0], {animate: false});
  }
  gBrowser.selectedBrowser.loadURI("about:blank");
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
