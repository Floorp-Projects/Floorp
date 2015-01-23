let gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
let gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(Ci.nsIHandlerService);

function setupFakeHandler() {
  let info = gMimeSvc.getFromTypeAndExtension("text/plain", "foo.txt");
  ok(info.possibleLocalHandlers.length, "Should have at least one known handler");
  let handler = info.possibleLocalHandlers.queryElementAt(0, Ci.nsILocalHandlerApp);

  let infoToModify = gMimeSvc.getFromTypeAndExtension("text/x-test-handler", null);
  infoToModify.possibleApplicationHandlers.appendElement(handler, false);

  gHandlerSvc.store(infoToModify);
}

function promisePopupEvent(popup, ev) {
  return new Promise((resolve, reject) => {
    popup.addEventListener(ev, () => {
      popup.removeEventListener(ev, arguments.callee);
      resolve();
    }, false);
  });
}

add_task(function*() {
  setupFakeHandler();
  yield openPreferencesViaOpenPreferencesAPI("applications", null, {leaveOpen: true});
  let win = gBrowser.selectedBrowser.contentWindow;

  let container = win.document.getElementById("handlersView");
  let ourItem = container.querySelector("richlistitem[type='text/x-test-handler']");
  ok(ourItem, "handlersView is present");
  ourItem.scrollIntoView();
  container.selectItem(ourItem);
  ok(ourItem.selected, "Should be able to select our item.");

  let list = yield waitForCondition(() => win.document.getAnonymousElementByAttribute(ourItem, "class", "actionsMenu"));
  let popup = list.firstChild;
  let popupShownPromise = promisePopupEvent(popup, "popupshown");
  list.boxObject.openMenu(true);
  yield popupShownPromise;

  let dialogLoadedPromise = promiseLoadSubDialog("chrome://global/content/appPicker.xul");
  let popupHiddenPromise = promisePopupEvent(popup, "popuphidden");

  let chooseItem = popup.querySelector(".choose-app-item");
  chooseItem.click();
  popup.hidePopup(); // Not clear why we need to do this manually, but we do. :-\

  yield popupHiddenPromise;
  let dialog = yield dialogLoadedPromise;
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
  ok(list.selectedItem, "Should have a selected item");
  ok(mimeInfo.preferredApplicationHandler.equals(list.selectedItem.handlerApp),
     "App should be visible as preferred item.");


  // Now try to 'manage' this list:
  popup = list.firstChild;
  popupShownPromise = promisePopupEvent(popup, "popupshown");
  list.boxObject.openMenu(true);
  yield popupShownPromise;

  dialogLoadedPromise = promiseLoadSubDialog("chrome://browser/content/preferences/applicationManager.xul");
  popupHiddenPromise = promisePopupEvent(popup, "popuphidden");

  let manageItem = popup.querySelector(".manage-app-item");
  manageItem.click();
  popup.hidePopup();

  yield popupHiddenPromise;
  dialog = yield dialogLoadedPromise;
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

