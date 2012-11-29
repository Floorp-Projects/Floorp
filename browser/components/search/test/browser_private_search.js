// This test performs a search in a public window, then a different
// search in a private window, and then checks in the public window
// whether there is an autocomplete entry for the private search.

function onLoad(callback) {
  gBrowser.addEventListener("DOMContentLoaded", function load() {
    gBrowser.removeEventListener("DOMContentLoaded", load, false);
    callback();
  }, false);
}

function doPrivateTest(searchBar) {
  gPrivateBrowsingUI.toggleMode();
  Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");

  searchBar.value = "p";
  searchBar.focus();
  let popup = searchBar.textbox.popup;
  //searchBar.textbox.closePopup();

  info("adding listener");
  popup.addEventListener("popupshowing", function showing() {
    let entries = getMenuEntries(searchBar);
    for (var i = 0; i < entries.length; i++)
      isnot(entries[i], "private test", "shouldn't see private autocomplete entries");
    popup.removeEventListener("popupshowing", showing, false);

    searchBar.textbox.toggleHistoryPopup();
    executeSoon(function() {
    searchBar.value = "";
    gBrowser.addTab();
    gBrowser.removeCurrentTab();
    content.location.href = "about:blank";
    var engine = Services.search.getEngineByName("Bug 426329");
    Services.search.removeEngine(engine);
    finish();
                  
                });
  }, false);

  info("triggering popup");
  searchBar.textbox.showHistoryPopup();  
}

function doTest() {
  let searchBar = BrowserSearch.searchBar;
  ok(searchBar, "got search bar");
  
  onLoad(function() {
    Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
    gPrivateBrowsingUI.toggleMode();

    onLoad(function() {
      doPrivateTest(searchBar);
    });
 
    searchBar.value = "private test";
    searchBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {});
  });
    
  searchBar.value = "public test";
  searchBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
}

function test() {
  waitForExplicitFinish();
  //gBrowser.addTab();
  //let testBrowser = gBrowser.selectedBrowser;
 
  function observer(aSub, aTopic, aData) {
    switch (aData) {
      case "engine-current":
        ok(ss.currentEngine.name == "Bug 426329", "currentEngine set");
        doTest();
        break;
      default:
      break;
    }
  }

  var ss = Services.search;
  Services.obs.addObserver(observer, "browser-search-engine-modified", false);
  ss.addEngine("http://mochi.test:8888/browser/browser/components/search/test/426329.xml",
               Ci.nsISearchEngine.DATA_XML, "data:image/x-icon,%00",
               false);
}

function getMenuEntries(searchBar) {
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
