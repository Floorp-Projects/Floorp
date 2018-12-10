/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

add_task(async function() {
  const ss = Services.search;
  const ENGINE_NAME = "Foo";
  let contextMenu;

  // We want select events to be fired.
  await SpecialPowers.pushPrefEnv({set: [["dom.select_events.enabled", true]]});

  let envService = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  let originalValue = envService.get("XPCSHELL_TEST_PROFILE_DIR");
  envService.set("XPCSHELL_TEST_PROFILE_DIR", "1");

  let url = "chrome://mochitests/content/browser/browser/components/search/test/browser/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  let originalSubstitution = resProt.getSubstitution("search-plugins");
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url));

  let searchDonePromise;
  await new Promise(resolve => {
    function observer(aSub, aTopic, aData) {
      switch (aData) {
        case "engine-added":
          let engine = ss.getEngineByName(ENGINE_NAME);
          ok(engine, "Engine was added.");
          ss.defaultEngine = engine;
          envService.set("XPCSHELL_TEST_PROFILE_DIR", originalValue);
          resProt.setSubstitution("search-plugins", originalSubstitution);
          break;
        case "engine-current":
          is(ss.defaultEngine.name, ENGINE_NAME, "currentEngine set");
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
                 "data:image/x-icon,%00", false);
  });

  contextMenu = document.getElementById("contentAreaContextMenu");
  ok(contextMenu, "Got context menu XUL");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/plain;charset=utf8,test%20search");

  await ContentTask.spawn(tab.linkedBrowser, "", async function() {
    return new Promise(resolve => {
      content.document.addEventListener("selectionchange", function() {
        resolve();
      }, {once: true});
      content.document.getSelection().selectAllChildren(content.document.body);
    });
  });

  let eventDetails = { type: "contextmenu", button: 2 };

  let popupPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter("body", eventDetails, gBrowser.selectedBrowser);
  await popupPromise;

  info("checkContextMenu");
  let searchItem = contextMenu.getElementsByAttribute("id", "context-searchselect")[0];
  ok(searchItem, "Got search context menu item");
  is(searchItem.label, "Search " + ENGINE_NAME + " for \u201ctest search\u201d", "Check context menu label");
  is(searchItem.disabled, false, "Check that search context menu item is enabled");

  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    searchItem.click();
  });

  is(gBrowser.currentURI.spec,
     "http://mochi.test:8888/browser/browser/components/search/test/browser/?test=test+search&ie=utf-8&channel=contextsearch",
     "Checking context menu search URL");

  contextMenu.hidePopup();

  // Remove the tab opened by the search
  gBrowser.removeCurrentTab();

  await new Promise(resolve => {
    searchDonePromise = resolve;
    ss.removeEngine(ss.defaultEngine);
  });

  gBrowser.removeCurrentTab();
});
