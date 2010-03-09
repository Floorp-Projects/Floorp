function test() {
  waitForExplicitFinish();

  var searchBar = BrowserSearch.searchBar;
  var searchButton = document.getAnonymousElementByAttribute(searchBar,
                     "anonid", "search-go-button");
  ok(searchButton, "got search-go-button");

  searchBar.value = "test";

  var obs = Cc["@mozilla.org/observer-service;1"].
            getService(Ci.nsIObserverService);
  var ss = Cc["@mozilla.org/browser/search-service;1"].
           getService(Ci.nsIBrowserSearchService);

  function observer(aSub, aTopic, aData) {
    switch (aData) {
      case "engine-added":
        var engine = ss.getEngineByName("Bug 426329");
        ok(engine, "Engine was added.");
        //XXX Bug 493051
        //ss.currentEngine = engine;
        break;
      case "engine-current":
        ok(ss.currentEngine.name == "Bug 426329", "currentEngine set");
        testReturn();
        break;
      case "engine-removed":
        obs.removeObserver(observer, "browser-search-engine-modified");
        finish();
        break;
    }
  }

  obs.addObserver(observer, "browser-search-engine-modified", false);
  ss.addEngine("http://localhost:8888/browser/browser/components/search/test/426329.xml",
               Ci.nsISearchEngine.DATA_XML, "data:image/x-icon,%00",
               false);

  var preSelectedBrowser, preTabNo;
  function init() {
    preSelectedBrowser = gBrowser.selectedBrowser;
    preTabNo = gBrowser.mTabs.length;
    searchBar.focus();
  }

  function testReturn() {
    init();
    EventUtils.synthesizeKey("VK_RETURN", {});
    doOnloadOnce(function(event) {

      is(gBrowser.mTabs.length, preTabNo, "Return key did not open new tab");
      is(event.originalTarget, preSelectedBrowser.contentDocument,
         "Return key loaded results in current tab");

      testAltReturn();
    });
  }

  function testAltReturn() {
    init();
    EventUtils.synthesizeKey("VK_RETURN", { altKey: true });
    doOnloadOnce(function(event) {

      is(gBrowser.mTabs.length, preTabNo + 1, "Alt+Return key added new tab");
      isnot(event.originalTarget, preSelectedBrowser.contentDocument,
            "Alt+Return key loaded results in new tab");
      is(event.originalTarget, gBrowser.contentDocument,
         "Alt+Return key loaded results in foreground tab");

      //Shift key has no effect for now, so skip it
      //testShiftAltReturn();
      testLeftClick();
    });
  }

  function testShiftAltReturn() {
    init();
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true, altKey: true });
    doOnloadOnce(function(event) {

      is(gBrowser.mTabs.length, preTabNo + 1, "Shift+Alt+Return key added new tab");
      isnot(event.originalTarget, preSelectedBrowser.contentDocument,
            "Shift+Alt+Return key loaded results in new tab");
      isnot(event.originalTarget, gBrowser.contentDocument,
            "Shift+Alt+Return key loaded results in background tab");

      testLeftClick();
    });
  }

  function testLeftClick() {
    init();
    simulateClick({ button: 0 }, searchButton);
    doOnloadOnce(function(event) {

      is(gBrowser.mTabs.length, preTabNo, "LeftClick did not open new tab");
      is(event.originalTarget, preSelectedBrowser.contentDocument,
         "LeftClick loaded results in current tab");

      testMiddleClick();
    });
  }

  function testMiddleClick() {
    init();
    simulateClick({ button: 1 }, searchButton);
    doOnloadOnce(function(event) {

      is(gBrowser.mTabs.length, preTabNo + 1, "MiddleClick added new tab");
      isnot(event.originalTarget, preSelectedBrowser.contentDocument,
            "MiddleClick loaded results in new tab");
      is(event.originalTarget, gBrowser.contentDocument,
         "MiddleClick loaded results in foreground tab");

      testShiftMiddleClick();
    });
  }

  function testShiftMiddleClick() {
    init();
    simulateClick({ button: 1, shiftKey: true }, searchButton);
    doOnloadOnce(function(event) {

      is(gBrowser.mTabs.length, preTabNo + 1, "Shift+MiddleClick added new tab");
      isnot(event.originalTarget, preSelectedBrowser.contentDocument,
            "Shift+MiddleClick loaded results in new tab");
      isnot(event.originalTarget, gBrowser.contentDocument,
            "Shift+MiddleClick loaded results in background tab");

      testRightClick();
    });
  }

  function testRightClick() {
    init();
    content.location.href = "about:blank";
    simulateClick({ button: 2 }, searchButton);
    setTimeout(function() {

      is(gBrowser.mTabs.length, preTabNo, "RightClick did not open new tab");
      is(gBrowser.currentURI.spec, "about:blank", "RightClick did nothing");

      finalize();
    }, 5000);
  }

  function finalize() {
    searchBar.value = "";
    while (gBrowser.mTabs.length != 1) {
      gBrowser.removeTab(gBrowser.mTabs[0]);
    }
    content.location.href = "about:blank";
    var engine = ss.getEngineByName("Bug 426329");
    ss.removeEngine(engine);
  }

  function doOnloadOnce(callback) {
    gBrowser.addEventListener("DOMContentLoaded", function(event) {
      gBrowser.removeEventListener("DOMContentLoaded", arguments.callee, true);
      callback(event);
    }, true);
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
}

