var gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
var gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

SimpleTest.requestCompleteLog();

function setupFakeHandler() {
  let info = gMimeSvc.getFromTypeAndExtension("text/plain", "foo.txt");
  ok(
    info.possibleLocalHandlers.length,
    "Should have at least one known handler"
  );
  let handler = info.possibleLocalHandlers.queryElementAt(
    0,
    Ci.nsILocalHandlerApp
  );

  let infoToModify = gMimeSvc.getFromTypeAndExtension(
    "text/x-test-handler",
    null
  );
  infoToModify.possibleApplicationHandlers.appendElement(handler);

  gHandlerSvc.store(infoToModify);
}

add_task(async function() {
  setupFakeHandler();

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");
  let win = gBrowser.selectedBrowser.contentWindow;

  let container = win.document.getElementById("handlersView");
  let ourItem = container.querySelector(
    "richlistitem[type='text/x-test-handler']"
  );
  ok(ourItem, "handlersView is present");
  ourItem.scrollIntoView();
  container.selectItem(ourItem);
  ok(ourItem.selected, "Should be able to select our item.");

  let list = ourItem.querySelector(".actionsMenu");

  let chooseItem = list.menupopup.querySelector(".choose-app-item");
  let dialogLoadedPromise = promiseLoadSubDialog(
    "chrome://global/content/appPicker.xhtml"
  );
  let cmdEvent = win.document.createEvent("xulcommandevent");
  cmdEvent.initCommandEvent(
    "command",
    true,
    true,
    win,
    0,
    false,
    false,
    false,
    false,
    0,
    null,
    0
  );
  chooseItem.dispatchEvent(cmdEvent);

  let dialog = await dialogLoadedPromise;
  info("Dialog loaded");

  let dialogDoc = dialog.document;
  let dialogElement = dialogDoc.getElementById("app-picker");
  let dialogList = dialogDoc.getElementById("app-picker-listbox");
  dialogList.selectItem(dialogList.firstElementChild);
  let selectedApp = dialogList.firstElementChild.handlerApp;
  dialogElement.acceptDialog();

  // Verify results are correct in mime service:
  let mimeInfo = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  ok(
    mimeInfo.preferredApplicationHandler.equals(selectedApp),
    "App should be set as preferred."
  );

  // Check that we display this result:
  ok(list.selectedItem, "Should have a selected item");
  ok(
    mimeInfo.preferredApplicationHandler.equals(list.selectedItem.handlerApp),
    "App should be visible as preferred item."
  );

  // Now try to 'manage' this list:
  dialogLoadedPromise = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/applicationManager.xhtml"
  );

  let manageItem = list.menupopup.querySelector(".manage-app-item");
  cmdEvent = win.document.createEvent("xulcommandevent");
  cmdEvent.initCommandEvent(
    "command",
    true,
    true,
    win,
    0,
    false,
    false,
    false,
    false,
    0,
    null,
    0
  );
  manageItem.dispatchEvent(cmdEvent);

  dialog = await dialogLoadedPromise;
  info("Dialog loaded the second time");

  dialogDoc = dialog.document;
  dialogElement = dialogDoc.getElementById("appManager");
  dialogList = dialogDoc.getElementById("appList");
  let itemToRemove = dialogList.querySelector(
    'richlistitem > label[value="' + selectedApp.name + '"]'
  ).parentNode;
  dialogList.selectItem(itemToRemove);
  let itemsBefore = dialogList.children.length;
  dialogDoc.getElementById("remove").click();
  ok(!itemToRemove.parentNode, "Item got removed from DOM");
  is(dialogList.children.length, itemsBefore - 1, "Item got removed");
  dialogElement.acceptDialog();

  // Verify results are correct in mime service:
  mimeInfo = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  ok(
    !mimeInfo.preferredApplicationHandler,
    "App should no longer be set as preferred."
  );

  // Check that we display this result:
  ok(list.selectedItem, "Should have a selected item");
  ok(
    !list.selectedItem.handlerApp,
    "No app should be visible as preferred item."
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

registerCleanupFunction(function() {
  let infoToModify = gMimeSvc.getFromTypeAndExtension(
    "text/x-test-handler",
    null
  );
  gHandlerSvc.remove(infoToModify);
});
