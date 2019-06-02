/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

const ENGINE_NAME = "mozSearch";
const ENGINE_ID = "mozsearch-engine@search.mozilla.org";

add_task(async function() {
  // We want select events to be fired.
  await SpecialPowers.pushPrefEnv({set: [["dom.select_events.enabled", true]]});

  await Services.search.init();

  // replace the path we load search engines from with
  // the path to our test data.
  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("mozsearch");
  let resProt = Services.io.getProtocolHandler("resource")
                           .QueryInterface(Ci.nsIResProtocolHandler);
  let originalSubstitution = resProt.getSubstitution("search-extensions");
  resProt.setSubstitution("search-extensions",
                          Services.io.newURI("file://" + searchExtensions.path));

  await Services.search.ensureBuiltinExtension(ENGINE_ID);

  let engine = await Services.search.getEngineByName(ENGINE_NAME);
  ok(engine, "Got a search engine");
  let defaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  let contextMenu = document.getElementById("contentAreaContextMenu");
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
     "https://example.com/browser/browser/components/search/test/browser/?test=test+search&ie=utf-8&channel=contextsearch",
     "Checking context menu search URL");

  contextMenu.hidePopup();

  // Remove the tab opened by the search
  gBrowser.removeCurrentTab();

  await Services.search.setDefault(defaultEngine);
  await Services.search.removeEngine(engine);

  resProt.setSubstitution("search-extensions", originalSubstitution);

  gBrowser.removeCurrentTab();
});
