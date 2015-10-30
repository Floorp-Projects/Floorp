/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

function test() {
  waitForExplicitFinish();

  const ss = Services.search;
  const ENGINE_NAME = "Foo";
  var contextMenu;

  let envService = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  let originalValue = envService.get("XPCSHELL_TEST_PROFILE_DIR");
  envService.set("XPCSHELL_TEST_PROFILE_DIR", "1");

  let url = "chrome://mochitests/content/browser/browser/components/search/test/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  let originalSubstitution = resProt.getSubstitution("search-plugins");
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url, null, null));

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
        startTest();
        break;
      case "engine-removed":
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        finish();
        break;
    }
  }

  Services.obs.addObserver(observer, "browser-search-engine-modified", false);
  ss.addEngine("resource://search-plugins/testEngine_mozsearch.xml",
               null, "data:image/x-icon,%00", false);

  function startTest() {
    contextMenu = document.getElementById("contentAreaContextMenu");
    ok(contextMenu, "Got context menu XUL");

    doOnloadOnce(testContextMenu);
    let tab = gBrowser.selectedTab = gBrowser.addTab("data:text/plain;charset=utf8,test%20search");
    registerCleanupFunction(function () {
      gBrowser.removeTab(tab);
    });
  }

  function testContextMenu() {
    function rightClickOnDocument() {
      info("rightClickOnDocument: " + content.window.location);
      waitForBrowserContextMenu(checkContextMenu);
      var clickTarget = content.document.body;
      var eventDetails = { type: "contextmenu", button: 2 };
      EventUtils.synthesizeMouseAtCenter(clickTarget, eventDetails, content);
    }

    // check the search menu item and then perform a search
    function checkContextMenu() {
      info("checkContextMenu");
      var searchItem = contextMenu.getElementsByAttribute("id", "context-searchselect")[0];
      ok(searchItem, "Got search context menu item");
      is(searchItem.label, 'Search ' + ENGINE_NAME + ' for "test search"', "Check context menu label");
      is(searchItem.disabled, false, "Check that search context menu item is enabled");
      doOnloadOnce(checkSearchURL);
      searchItem.click();
      contextMenu.hidePopup();
    }

    function checkSearchURL(event) {
      is(event.originalTarget.URL,
         "http://mochi.test:8888/browser/browser/components/search/test/?test=test+search&ie=utf-8&channel=contextsearch",
         "Checking context menu search URL");
      // Remove the tab opened by the search
      gBrowser.removeCurrentTab();
      ss.removeEngine(ss.currentEngine);
    }

    var selectionListener = {
      notifySelectionChanged: function(doc, sel, reason) {
        if (reason != Ci.nsISelectionListener.SELECTALL_REASON || sel.toString() != "test search")
          return;
        info("notifySelectionChanged: Text selected");
        content.window.getSelection().QueryInterface(Ci.nsISelectionPrivate).
                                      removeSelectionListener(selectionListener);
        SimpleTest.executeSoon(rightClickOnDocument);
      }
    };

    // Delay the select all to avoid intermittent selection failures.
    setTimeout(function delaySelectAll() {
      info("delaySelectAll: " + content.window.location.toString());
      // add a listener to know when the selection takes effect
      content.window.getSelection().QueryInterface(Ci.nsISelectionPrivate).
                                    addSelectionListener(selectionListener);
      // select the text on the page
      goDoCommand('cmd_selectAll');
    }, 500);
  }
}
