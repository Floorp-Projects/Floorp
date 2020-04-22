/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

SimpleTest.requestCompleteLog();
ChromeUtils.import(
  "resource://testing-common/HandlerServiceTestUtils.jsm",
  this
);

let gHandlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

let gOldMailHandlers = [];
let gDummyHandlers = [];
let gOriginalPreferredMailHandler;
let gOriginalPreferredPDFHandler;

registerCleanupFunction(function() {
  function removeDummyHandlers(handlers) {
    // Remove any of the dummy handlers we created.
    for (let i = handlers.Count() - 1; i >= 0; i--) {
      try {
        if (
          gDummyHandlers.some(
            h =>
              h.uriTemplate ==
              handlers.queryElementAt(i, Ci.nsIWebHandlerApp).uriTemplate
          )
        ) {
          handlers.removeElementAt(i);
        }
      } catch (ex) {
        /* ignore non-web-app handlers */
      }
    }
  }
  // Re-add the original protocol handlers:
  let mailHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("mailto");
  let mailHandlers = mailHandlerInfo.possibleApplicationHandlers;
  for (let h of gOldMailHandlers) {
    mailHandlers.appendElement(h);
  }
  removeDummyHandlers(mailHandlers);
  mailHandlerInfo.preferredApplicationHandler = gOriginalPreferredMailHandler;
  gHandlerService.store(mailHandlerInfo);

  let pdfHandlerInfo = HandlerServiceTestUtils.getHandlerInfo(
    "application/pdf"
  );
  pdfHandlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
  pdfHandlerInfo.preferredApplicationHandler = gOriginalPreferredPDFHandler;
  let handlers = pdfHandlerInfo.possibleApplicationHandlers;
  for (let i = handlers.Count() - 1; i >= 0; i--) {
    let app = handlers.queryElementAt(i, Ci.nsIHandlerApp);
    if (app.name == "Foopydoopydoo") {
      handlers.removeElementAt(i);
    }
  }
  gHandlerService.store(pdfHandlerInfo);

  gBrowser.removeCurrentTab();
});

function scrubMailtoHandlers(handlerInfo) {
  // Remove extant web handlers because they have icons that
  // we fetch from the web, which isn't allowed in tests.
  let handlers = handlerInfo.possibleApplicationHandlers;
  for (let i = handlers.Count() - 1; i >= 0; i--) {
    try {
      let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
      gOldMailHandlers.push(handler);
      // If we get here, this is a web handler app. Remove it:
      handlers.removeElementAt(i);
    } catch (ex) {}
  }
}

("use strict");

add_task(async function setup() {
  // Create our dummy handlers
  let handler1 = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  handler1.name = "Handler 1";
  handler1.uriTemplate = "https://example.com/first/%s";

  let handler2 = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  handler2.name = "Handler 2";
  handler2.uriTemplate = "http://example.org/second/%s";
  gDummyHandlers.push(handler1, handler2);

  function substituteWebHandlers(handlerInfo) {
    // Append the dummy handlers to replace them:
    let handlers = handlerInfo.possibleApplicationHandlers;
    handlers.appendElement(handler1);
    handlers.appendElement(handler2);
    gHandlerService.store(handlerInfo);
  }
  // Set up our mailto handler test infrastructure.
  let mailtoHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("mailto");
  scrubMailtoHandlers(mailtoHandlerInfo);
  gOriginalPreferredMailHandler = mailtoHandlerInfo.preferredApplicationHandler;
  substituteWebHandlers(mailtoHandlerInfo);

  // Now add a pdf handler:
  let pdfHandlerInfo = HandlerServiceTestUtils.getHandlerInfo(
    "application/pdf"
  );
  // PDF doesn't have built-in web handlers, so no need to scrub.
  gOriginalPreferredPDFHandler = pdfHandlerInfo.preferredApplicationHandler;
  let handlers = pdfHandlerInfo.possibleApplicationHandlers;
  let appHandler = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  appHandler.name = "Foopydoopydoo";
  appHandler.executable = Services.dirsvc.get("ProfD", Ci.nsIFile);
  appHandler.executable.append("dummy.exe");
  // Prefs are picky and want this to exist and be executable (bug 1626009):
  if (!appHandler.executable.exists()) {
    appHandler.executable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o777);
  }

  handlers.appendElement(appHandler);

  pdfHandlerInfo.preferredApplicationHandler = appHandler;
  pdfHandlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  gHandlerService.store(pdfHandlerInfo);

  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  info("Preferences page opened on the general pane.");

  await gBrowser.selectedBrowser.contentWindow.promiseLoadHandlersList;
  info("Apps list loaded.");
});

add_task(async function dialogShowsCorrectContent() {
  let win = gBrowser.selectedBrowser.contentWindow;

  let container = win.document.getElementById("handlersView");

  // First, find the PDF item.
  let pdfItem = container.querySelector("richlistitem[type='application/pdf']");
  Assert.ok(pdfItem, "pdfItem is present in handlersView.");
  pdfItem.scrollIntoView({ block: "center" });
  pdfItem.closest("richlistbox").selectItem(pdfItem);

  // Open its menu
  let list = pdfItem.querySelector(".actionsMenu");
  let popup = list.menupopup;
  let popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(list, {}, win);
  await popupShown;

  // Then open the dialog
  const promiseDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/applicationManager.xhtml"
  );
  EventUtils.synthesizeMouseAtCenter(
    popup.querySelector(".manage-app-item"),
    {},
    win
  );
  let dialogWin = await promiseDialogLoaded;

  // Then verify that the description is correct.
  let desc = dialogWin.document.getElementById("appDescription");
  let descL10n = dialogWin.document.l10n.getAttributes(desc);
  is(descL10n.id, "app-manager-handle-file", "Should have right string");
  is(
    descL10n.args.type,
    await dialogWin.document.l10n.formatValue("applications-type-pdf"),
    "Should have PDF string bits."
  );

  // And that there's one item in the list, with the correct name:
  let appList = dialogWin.document.getElementById("appList");
  is(appList.itemCount, 1, "Should have 1 item in the list");
  is(
    appList.selectedItem.querySelector("label").getAttribute("value"),
    "Foopydoopydoo",
    "Should have the right executable label"
  );

  dialogWin.close();
});
