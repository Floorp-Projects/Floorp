/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);

const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

// This test checks that application/xml has the handle internally option.
add_task(async function applicationXmlHandleInternally() {
  const mimeInfo = MIMEService.getFromTypeAndExtension(
    "application/xml",
    "xml"
  );
  HandlerService.store(mimeInfo);
  registerCleanupFunction(() => {
    HandlerService.remove(mimeInfo);
  });

  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });

  let win = gBrowser.selectedBrowser.contentWindow;

  let container = win.document.getElementById("handlersView");

  // First, find the application/xml item.
  let xmlItem = container.querySelector("richlistitem[type='application/xml']");
  Assert.ok(xmlItem, "application/xml is present in handlersView");
  if (xmlItem) {
    xmlItem.scrollIntoView({ block: "center" });
    xmlItem.closest("richlistbox").selectItem(xmlItem);

    // Open its menu
    let list = xmlItem.querySelector(".actionsMenu");
    let popup = list.menupopup;
    let popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(list, {}, win);
    await popupShown;

    let handleInternallyItem = list.querySelector(
      `menuitem[action='${Ci.nsIHandlerInfo.handleInternally}']`
    );

    ok(!!handleInternallyItem, "handle internally is present");
  }

  gBrowser.removeCurrentTab();
});
