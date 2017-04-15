/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

add_task(function* () {
  const ss = Services.search;
  const ENGINE_NAME = "Foo";
  var contextMenu;

  // We want select events to be fired.
  yield SpecialPowers.pushPrefEnv({set: [["dom.select_events.enabled", true]]});

  let envService = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  let originalValue = envService.get("XPCSHELL_TEST_PROFILE_DIR");
  envService.set("XPCSHELL_TEST_PROFILE_DIR", "1");

  let url = "chrome://mochitests/content/browser/browser/components/search/test/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  let originalSubstitution = resProt.getSubstitution("search-plugins");
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url));

  let searchDonePromise;
  yield new Promise(resolve => {
    function observer(aSub, aTopic, aData) {
      switch (aData) {
        case "engine-added":
          var engine = ss.getEngineByName(ENGINE_NAME);
          ok(engine, "Engine was added.");
          ss.currentEngine = engine;
          envService.set("XPCSHELL_TEST_PROFILE_DIR", originalValue);
          resProt.setSubstitution("search-plugins", originalSubstitution);
          break;
        case "engine-current":
          is(ss.currentEngine.name, ENGINE_NAME, "currentEngine set");
          resolve();
          break;
        case "engine-removed":
          Services.obs.removeObserver(observer, "browser-search-engine-modified");
          if (searchDonePromise) {
            searchDonePromise();
          }
          break;
      }
    }

    Services.obs.addObserver(observer, "browser-search-engine-modified");
    ss.addEngine("resource://search-plugins/testEngine_mozsearch.xml",
                 null, "data:image/x-icon,%00", false);
  });

  contextMenu = document.getElementById("contentAreaContextMenu");
  ok(contextMenu, "Got context menu XUL");

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/plain;charset=utf8,test%20search");

  yield ContentTask.spawn(tab.linkedBrowser, "", function*() {
    return new Promise(resolve => {
      content.document.addEventListener("selectionchange", function() {
        resolve();
      }, {once: true});
      content.document.getSelection().selectAllChildren(content.document.body);
    });
  });

  var eventDetails = { type: "contextmenu", button: 2 };

  let popupPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter("body", eventDetails, gBrowser.selectedBrowser);
  yield popupPromise;

  info("checkContextMenu");
  var searchItem = contextMenu.getElementsByAttribute("id", "context-searchselect")[0];
  ok(searchItem, "Got search context menu item");
  is(searchItem.label, "Search " + ENGINE_NAME + " for \u201ctest search\u201d", "Check context menu label");
  is(searchItem.disabled, false, "Check that search context menu item is enabled");

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    searchItem.click();
  });

  is(gBrowser.currentURI.spec,
     "http://mochi.test:8888/browser/browser/components/search/test/?test=test+search&ie=utf-8&channel=contextsearch",
     "Checking context menu search URL");

  contextMenu.hidePopup();

  // Remove the tab opened by the search
  gBrowser.removeCurrentTab();

  yield new Promise(resolve => {
    searchDonePromise = resolve;
    ss.removeEngine(ss.currentEngine);
  });

  gBrowser.removeCurrentTab();
});
