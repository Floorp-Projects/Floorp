// Instead of loading ChromeUtils.js into the test scope in browser-test.js for all tests,
// we only need ChromeUtils.js for a few files which is why we are using loadSubScript.
var ChromeUtils = {};
this._scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
  "resource://gre/modules/FormHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

function expectedURL(aSearchTerms) {
  const ENGINE_HTML_BASE = "http://mochi.test:8888/browser/browser/components/search/test/test.html";
  var textToSubURI = Cc["@mozilla.org/intl/texttosuburi;1"].
                     getService(Ci.nsITextToSubURI);
  var searchArg = textToSubURI.ConvertAndEscape("utf-8", aSearchTerms);
  return ENGINE_HTML_BASE + "?test=" + searchArg;
}

function simulateClick(aEvent, aTarget) {
  var event = document.createEvent("MouseEvent");
  var ctrlKeyArg  = aEvent.ctrlKey  || false;
  var altKeyArg   = aEvent.altKey   || false;
  var shiftKeyArg = aEvent.shiftKey || false;
  var metaKeyArg  = aEvent.metaKey  || false;
  var buttonArg   = aEvent.button   || 0;
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
  var entries = [];
  var autocompleteMenu = searchBar.textbox.popup;
  // Could perhaps pull values directly from the controller, but it seems
  // more reliable to test the values that are actually in the tree?
  var column = autocompleteMenu.tree.columns[0];
  var numRows = autocompleteMenu.tree.view.rowCount;
  for (var i = 0; i < numRows; i++) {
    entries.push(autocompleteMenu.tree.view.getValueAt(i, column));
  }
  return entries;
}

function* countEntries(name, value) {
  let deferred = Promise.defer();
  let count = 0;
  let obj = name && value ? {fieldname: name, value: value} : {};
  FormHistory.count(obj,
                    { handleResult: function(result) { count = result; },
                      handleError: function(error) { throw error; },
                      handleCompletion: function(reason) {
                        if (!reason) {
                          deferred.resolve(count);
                        }
                      }
                    });
  return deferred.promise;
}

var searchBar;
var searchButton;
var searchEntries = ["test", "More Text", "Some Text"];
function* promiseSetEngine() {
  let deferred = Promise.defer();
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
        deferred.resolve();
        break;
    }
  };

  Services.obs.addObserver(observer, "browser-search-engine-modified", false);
  ss.addEngine("http://mochi.test:8888/browser/browser/components/search/test/426329.xml",
               Ci.nsISearchEngine.DATA_XML, "data:image/x-icon,%00",
               false);

  return deferred.promise;
}

function* promiseRemoveEngine() {
  let deferred = Promise.defer();
  var ss = Services.search;

  function observer(aSub, aTopic, aData) {
    if (aData == "engine-removed") {
      Services.obs.removeObserver(observer, "browser-search-engine-modified");
      deferred.resolve();
    }
  };

  Services.obs.addObserver(observer, "browser-search-engine-modified", false);
  var engine = ss.getEngineByName("Bug 426329");
  ss.removeEngine(engine);

  return deferred.promise;
}


var preSelectedBrowser;
var preTabNo;
function* prepareTest() {
  preSelectedBrowser = gBrowser.selectedBrowser;
  preTabNo = gBrowser.tabs.length;
  searchBar = BrowserSearch.searchBar;

  let windowFocused = Promise.defer();
  SimpleTest.waitForFocus(windowFocused.resolve, window);
  yield windowFocused.promise;

  let deferred = Promise.defer();
  if (document.activeElement != searchBar) {
    searchBar.addEventListener("focus", function onFocus() {
      searchBar.removeEventListener("focus", onFocus);
      deferred.resolve();
    });
    searchBar.focus();
  } else {
    deferred.resolve();
  }
  return deferred.promise;
}

add_task(function testSetupEngine() {
  yield promiseSetEngine();
});

add_task(function testReturn() {
  yield prepareTest();
  EventUtils.synthesizeKey("VK_RETURN", {});
  let event = yield promiseOnLoad();

  is(gBrowser.tabs.length, preTabNo, "Return key did not open new tab");
  is(event.originalTarget, preSelectedBrowser.contentDocument,
     "Return key loaded results in current tab");
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testReturn opened correct search page");
});

add_task(function testAltReturn() {
  yield prepareTest();
  EventUtils.synthesizeKey("VK_RETURN", { altKey: true });
  let event = yield promiseOnLoad();

  is(gBrowser.tabs.length, preTabNo + 1, "Alt+Return key added new tab");
  isnot(event.originalTarget, preSelectedBrowser.contentDocument,
        "Alt+Return key loaded results in new tab");
  is(event.originalTarget, gBrowser.contentDocument,
     "Alt+Return key loaded results in foreground tab");
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testAltReturn opened correct search page");
});

//Shift key has no effect for now, so skip it
add_task(function testShiftAltReturn() {
  return;

  yield prepareTest();
  EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true, altKey: true });
  let event = yield promiseOnLoad();

  is(gBrowser.tabs.length, preTabNo + 1, "Shift+Alt+Return key added new tab");
  isnot(event.originalTarget, preSelectedBrowser.contentDocument,
        "Shift+Alt+Return key loaded results in new tab");
  isnot(event.originalTarget, gBrowser.contentDocument,
        "Shift+Alt+Return key loaded results in background tab");
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testShiftAltReturn opened correct search page");
});

add_task(function testLeftClick() {
  yield prepareTest();
  simulateClick({ button: 0 }, searchButton);
  let event = yield promiseOnLoad();
  is(gBrowser.tabs.length, preTabNo, "LeftClick did not open new tab");
  is(event.originalTarget, preSelectedBrowser.contentDocument,
     "LeftClick loaded results in current tab");
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testLeftClick opened correct search page");
});

add_task(function testMiddleClick() {
  yield prepareTest();
  simulateClick({ button: 1 }, searchButton);
  let event = yield promiseOnLoad();
  is(gBrowser.tabs.length, preTabNo + 1, "MiddleClick added new tab");
  isnot(event.originalTarget, preSelectedBrowser.contentDocument,
        "MiddleClick loaded results in new tab");
  is(event.originalTarget, gBrowser.contentDocument,
     "MiddleClick loaded results in foreground tab");
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testMiddleClick opened correct search page");
});

add_task(function testShiftMiddleClick() {
  yield prepareTest();
  simulateClick({ button: 1, shiftKey: true }, searchButton);
  let event = yield promiseOnLoad();
  is(gBrowser.tabs.length, preTabNo + 1, "Shift+MiddleClick added new tab");
  isnot(event.originalTarget, preSelectedBrowser.contentDocument,
        "Shift+MiddleClick loaded results in new tab");
  isnot(event.originalTarget, gBrowser.contentDocument,
        "Shift+MiddleClick loaded results in background tab");
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testShiftMiddleClick opened correct search page");
});

add_task(function testDropText() {
  yield prepareTest();
  let promisePreventPopup = promiseEvent(searchBar, "popupshowing", true);
  // drop on the search button so that we don't need to worry about the
  // default handlers for textboxes.
  ChromeUtils.synthesizeDrop(searchBar.searchButton, searchBar.searchButton, [[ {type: "text/plain", data: "Some Text" } ]], "copy", window);
  yield promisePreventPopup;
  let event = yield promiseOnLoad();
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testDropText opened correct search page");
  is(searchBar.value, "Some Text", "drop text/plain on searchbar");
});

add_task(function testDropInternalText() {
  yield prepareTest();
  let promisePreventPopup = promiseEvent(searchBar, "popupshowing", true);
  ChromeUtils.synthesizeDrop(searchBar.searchButton, searchBar.searchButton, [[ {type: "text/x-moz-text-internal", data: "More Text" } ]], "copy", window);
  yield promisePreventPopup;
  let event = yield promiseOnLoad();
  is(event.originalTarget.URL, expectedURL(searchBar.value), "testDropInternalText opened correct search page");
  is(searchBar.value, "More Text", "drop text/x-moz-text-internal on searchbar");

  // testDropLink implicitly depended on testDropInternalText, so these two tests
  // were merged so that if testDropInternalText failed it wouldn't cause testDropLink
  // to fail unexplainably.
  yield prepareTest();
  let promisePreventPopup = promiseEvent(searchBar, "popupshowing", true);
  ChromeUtils.synthesizeDrop(searchBar.searchButton, searchBar.searchButton, [[ {type: "text/uri-list", data: "http://www.mozilla.org" } ]], "copy", window);
  yield promisePreventPopup;
  is(searchBar.value, "More Text", "drop text/uri-list on searchbar shouldn't change anything");
});

add_task(function testRightClick() {
  preTabNo = gBrowser.tabs.length;
  content.location.href = "about:blank";
  simulateClick({ button: 2 }, searchButton);
  let deferred = Promise.defer();
  setTimeout(function() {
    is(gBrowser.tabs.length, preTabNo, "RightClick did not open new tab");
    is(gBrowser.currentURI.spec, "about:blank", "RightClick did nothing");
    deferred.resolve();
  }, 5000);
  yield deferred.promise;
});

add_task(function testSearchHistory() {
  var textbox = searchBar._textbox;
  for (var i = 0; i < searchEntries.length; i++) {
    let count = yield countEntries(textbox.getAttribute("autocompletesearchparam"), searchEntries[i]);
    ok(count > 0, "form history entry '" + searchEntries[i] + "' should exist");
  }
});

add_task(function testAutocomplete() {
  var popup = searchBar.textbox.popup;
  let popupShownPromise = promiseEvent(popup, "popupshown");
  searchBar.textbox.showHistoryPopup();
  yield popupShownPromise;
  checkMenuEntries(searchEntries);
});

add_task(function testClearHistory() {
  let controller = searchBar.textbox.controllers.getControllerForCommand("cmd_clearhistory")
  ok(controller.isCommandEnabled("cmd_clearhistory"), "Clear history command enabled");
  controller.doCommand("cmd_clearhistory");
  let count = yield countEntries();
  ok(count == 0, "History cleared");
});

add_task(function asyncCleanup() {
  searchBar.value = "";
  while (gBrowser.tabs.length != 1) {
    gBrowser.removeTab(gBrowser.tabs[0], {animate: false});
  }
  content.location.href = "about:blank";
  yield promiseRemoveEngine();
});

