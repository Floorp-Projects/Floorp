var gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
var gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(Ci.nsIHandlerService);

SimpleTest.requestCompleteLog();

function setupFakeHandler() {
  let info = gMimeSvc.getFromTypeAndExtension("text/plain", "foo.txt");
  ok(info.possibleLocalHandlers.length, "Should have at least one known handler");
  let handler = info.possibleLocalHandlers.queryElementAt(0, Ci.nsILocalHandlerApp);

  let infoToModify = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  infoToModify.possibleApplicationHandlers.appendElement(handler, false);

  gHandlerSvc.store(infoToModify);
}

add_task(function*() {
  setupFakeHandler();
  yield openPreferencesViaOpenPreferencesAPI("applications", {leaveOpen: true});
  info("Preferences page opened on the applications pane.");
  let win = gBrowser.selectedBrowser.contentWindow;

  let container = win.document.getElementById("handlersView");
  let ourItem = container.querySelector("richlistitem[type='text/x-test-handler']");
  ok(ourItem, "handlersView is present");
  ourItem.scrollIntoView();
  container.selectItem(ourItem);
  ok(ourItem.selected, "Should be able to select our item.");

  let list = yield waitForCondition(() => win.document.getAnonymousElementByAttribute(ourItem, "class", "actionsMenu"));
  info("Got list after item was selected");

  let chooseItem = list.firstChild.querySelector(".choose-app-item");
  let dialogLoadedPromise = promiseLoadSubDialog("chrome://global/content/appPicker.xul");
  let cmdEvent = win.document.createEvent("xulcommandevent");
  cmdEvent.initCommandEvent("command", true, true, win, 0, false, false, false, false, null);
  chooseItem.dispatchEvent(cmdEvent);

  let dialog = yield dialogLoadedPromise;
  info("Dialog loaded");

  let dialogDoc = dialog.document;
  let dialogList = dialogDoc.getElementById("app-picker-listbox");
  dialogList.selectItem(dialogList.firstChild);
  let selectedApp = dialogList.firstChild.handlerApp;
  dialogDoc.documentElement.acceptDialog();

  // Verify results are correct in mime service:
  let mimeInfo = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  ok(mimeInfo.preferredApplicationHandler.equals(selectedApp), "App should be set as preferred.");

  // Check that we display this result:
  list = yield waitForCondition(() => win.document.getAnonymousElementByAttribute(ourItem, "class", "actionsMenu"));
  info("Got list after item was selected");
  ok(list.selectedItem, "Should have a selected item");
  ok(mimeInfo.preferredApplicationHandler.equals(list.selectedItem.handlerApp),
     "App should be visible as preferred item.");


  // Now try to 'manage' this list:
  dialogLoadedPromise = promiseLoadSubDialog("chrome://browser/content/preferences/applicationManager.xul");

  let manageItem = list.firstChild.querySelector(".manage-app-item");
  cmdEvent = win.document.createEvent("xulcommandevent");
  cmdEvent.initCommandEvent("command", true, true, win, 0, false, false, false, false, null);
  manageItem.dispatchEvent(cmdEvent);

  dialog = yield dialogLoadedPromise;
  info("Dialog loaded the second time");

  dialogDoc = dialog.document;
  dialogList = dialogDoc.getElementById("appList");
  let itemToRemove = dialogList.querySelector('listitem[label="' + selectedApp.name + '"]');
  dialogList.selectItem(itemToRemove);
  let itemsBefore = dialogList.children.length;
  dialogDoc.getElementById("remove").click();
  ok(!itemToRemove.parentNode, "Item got removed from DOM");
  is(dialogList.children.length, itemsBefore - 1, "Item got removed");
  dialogDoc.documentElement.acceptDialog();

  // Verify results are correct in mime service:
  mimeInfo = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  ok(!mimeInfo.preferredApplicationHandler, "App should no longer be set as preferred.");

  // Check that we display this result:
  list = yield waitForCondition(() => win.document.getAnonymousElementByAttribute(ourItem, "class", "actionsMenu"));
  ok(list.selectedItem, "Should have a selected item");
  ok(!list.selectedItem.handlerApp,
     "No app should be visible as preferred item.");

  gBrowser.removeCurrentTab();
});

registerCleanupFunction(function() {
  let infoToModify = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  gHandlerSvc.remove(infoToModify);
});

