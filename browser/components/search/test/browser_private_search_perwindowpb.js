// This test performs a search in a public window, then a different
// search in a private window, and then checks in the public window
// whether there is an autocomplete entry for the private search.

function test() {
  // Don't use about:home as the homepage for new windows
  Services.prefs.setIntPref("browser.startup.page", 0);
  registerCleanupFunction(function() Services.prefs.clearUserPref("browser.startup.page"));

  waitForExplicitFinish();

  let engineURL =
    "http://mochi.test:8888/browser/browser/components/search/test/";
  let windowsToClose = [];
  registerCleanupFunction(function() {
    let engine = Services.search.getEngineByName("Bug 426329");
    Services.search.removeEngine(engine);
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  function onPageLoad(aWin, aCallback) {
    aWin.gBrowser.addEventListener("DOMContentLoaded", function load(aEvent) {
      let doc = aEvent.originalTarget;
      info(doc.location.href);
      if (doc.location.href.indexOf(engineURL) != -1) {
        aWin.gBrowser.removeEventListener("DOMContentLoaded", load, false);
        aCallback();
      }
    }, false);
  }

  function performSearch(aWin, aIsPrivate, aCallback) {
    let searchBar = aWin.BrowserSearch.searchBar;
    ok(searchBar, "got search bar");
    onPageLoad(aWin, aCallback);

    searchBar.value = aIsPrivate ? "private test" : "public test";
    searchBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {}, aWin);
  }

  function addEngine(aCallback) {
    let installCallback = {
      onSuccess: function (engine) {
        Services.search.currentEngine = engine;
        aCallback();
      },
      onError: function (errorCode) {
        ok(false, "failed to install engine: " + errorCode);
      }
    };
    Services.search.addEngine(engineURL + "426329.xml",
                              Ci.nsISearchEngine.DATA_XML,
                              "data:image/x-icon,%00", false, installCallback);
  }

  function testOnWindow(aIsPrivate, aCallback) {
    let win = whenNewWindowLoaded({ private: aIsPrivate }, function() {
      waitForFocus(aCallback, win);
    });
    windowsToClose.push(win);
  }

  addEngine(function() {
    testOnWindow(false, function(win) {
      performSearch(win, false, function() {
        testOnWindow(true, function(win) {
          performSearch(win, true, function() {
            testOnWindow(false, function(win) {
              checkSearchPopup(win, finish);
            });
          });
        });
      });
    });
  });
}

function checkSearchPopup(aWin, aCallback) {
  let searchBar = aWin.BrowserSearch.searchBar;
  searchBar.value = "p";
  searchBar.focus();

  let popup = searchBar.textbox.popup;
  popup.addEventListener("popupshowing", function showing() {
    popup.removeEventListener("popupshowing", showing, false);

    let entries = getMenuEntries(searchBar);
    for (let i = 0; i < entries.length; i++) {
      isnot(entries[i], "private test",
            "shouldn't see private autocomplete entries");
    }

    searchBar.textbox.toggleHistoryPopup();
    searchBar.value = "";
    aCallback();
  }, false);

  searchBar.textbox.showHistoryPopup();
}

function getMenuEntries(searchBar) {
  let entries = [];
  let autocompleteMenu = searchBar.textbox.popup;
  // Could perhaps pull values directly from the controller, but it seems
  // more reliable to test the values that are actually in the tree?
  let column = autocompleteMenu.tree.columns[0];
  let numRows = autocompleteMenu.tree.view.rowCount;
  for (let i = 0; i < numRows; i++) {
    entries.push(autocompleteMenu.tree.view.getValueAt(i, column));
  }
  return entries;
}
