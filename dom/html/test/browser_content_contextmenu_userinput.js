"use strict";

const kPage = "http://example.org/browser/" +
              "dom/html/test/file_content_contextmenu.html";

add_task(async function() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: kPage
  }, async function(aBrowser) {
    let contextMenu = document.getElementById("contentAreaContextMenu");
    ok(contextMenu, "Got context menu");

    info("Open context menu");
    is(contextMenu.state, "closed", "Should not have opened context menu");
    let popupShownPromise = promiseWaitForEvent(window, "popupshown");
    EventUtils.synthesizeMouse(aBrowser, window.innerWidth / 3,
                               window.innerHeight / 3,
                               {type: "contextmenu", button: 2}, window);
    await popupShownPromise;
    is(contextMenu.state, "open", "Should have opened context menu");

    let pageMenuSep = document.getElementById("page-menu-separator");
    ok(pageMenuSep && !pageMenuSep.hidden,
       "Page menu separator should be shown");

    let testMenuSep = pageMenuSep.previousSibling;
    ok(testMenuSep && !testMenuSep.hidden,
       "User-added menu separator should be shown");

    let testMenuItem = testMenuSep.previousSibling;
    is(testMenuItem.label, "Test Context Menu Click", "Got context menu item");

    let promiseCtxMenuClick = ContentTask.spawn(aBrowser, null, async function() {
      await new Promise(resolve => {
        let Ci = Components.interfaces;
        let windowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils);
        let menuitem = content.document.getElementById("menuitem");
        menuitem.addEventListener("click", function() {
          Assert.ok(windowUtils.isHandlingUserInput,
                    "Content menu click should be a user input");
          resolve();
        });
      });
    });
    EventUtils.synthesizeMouseAtCenter(testMenuItem, {}, window);
    await promiseCtxMenuClick;
  });
});
