"use strict";

function frameScript() {
  let Ci = Components.interfaces;
  let windowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
  let menuitem = content.document.getElementById("menuitem");
  menuitem.addEventListener("click", function() {
    sendAsyncMessage("Test:ContextMenuClick", windowUtils.isHandlingUserInput);
  });
}

var gMessageManager;

function listenOneMessage(aMsg, aListener) {
  function listener({ data }) {
    gMessageManager.removeMessageListener(aMsg, listener);
    aListener(data);
  }
  gMessageManager.addMessageListener(aMsg, listener);
}

function promiseOneMessage(aMsg) {
  return new Promise(resolve => listenOneMessage(aMsg, resolve));
}

const kPage = "http://example.org/browser/" +
              "dom/html/test/file_content_contextmenu.html";

add_task(function* () {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: kPage
  }, function*(aBrowser) {
    gMessageManager = aBrowser.messageManager;
    ContentTask.spawn(aBrowser, null, frameScript);

    let contextMenu = document.getElementById("contentAreaContextMenu");
    ok(contextMenu, "Got context menu");

    info("Open context menu");
    is(contextMenu.state, "closed", "Should not have opened context menu");
    let popupShownPromise = promiseWaitForEvent(window, "popupshown");
    EventUtils.synthesizeMouse(aBrowser, window.innerWidth / 3,
                               window.innerHeight / 3,
                               {type: "contextmenu", button: 2}, window);
    yield popupShownPromise;
    is(contextMenu.state, "open", "Should have opened context menu");

    let pageMenuSep = document.getElementById("page-menu-separator");
    ok(pageMenuSep && !pageMenuSep.hidden,
       "Page menu separator should be shown");
    let testMenuItem = pageMenuSep.previousSibling;
    is(testMenuItem.label, "Test Context Menu Click", "Got context menu item");

    let promiseContextMenuClick = promiseOneMessage("Test:ContextMenuClick");
    EventUtils.synthesizeMouseAtCenter(testMenuItem, {}, window);
    let isUserInput = yield promiseContextMenuClick;
    ok(isUserInput, "Content menu click should be a user input");
  });
});
