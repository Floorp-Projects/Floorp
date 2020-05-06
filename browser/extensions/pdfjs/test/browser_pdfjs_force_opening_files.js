/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_file_opening() {
  // Get a ref to the pdf we want to open.
  let dirFileObj = getChromeDir(getResolvedURI(gTestPath));
  dirFileObj.append("file_pdfjs_test.pdf");

  // Change the defaults.
  var oldAction = changeMimeHandler(Ci.nsIHandlerInfo.useSystemDefault, true);

  // Test: "Open with" dialog should not come up, despite pdf.js not being
  // the default - because files from disk should always use pdfjs, unless
  // it is forcibly disabled.
  let openedWindow = false;
  let windowOpenedPromise = new Promise((resolve, reject) => {
    addWindowListener(
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      () => {
        openedWindow = true;
        resolve();
      }
    );
  });

  // Open the tab with a system principal:
  var tab = BrowserTestUtils.addTab(gBrowser, dirFileObj.path);

  let pdfjsLoadedPromise = TestUtils.waitForCondition(() => {
    let { contentPrincipal } = tab.linkedBrowser;
    return (contentPrincipal?.URI?.spec || "").endsWith("viewer.html");
  });
  await Promise.race([pdfjsLoadedPromise, windowOpenedPromise]);
  ok(!openedWindow, "Shouldn't open an unknownContentType window!");

  registerCleanupFunction(function() {
    if (listenerCleanup) {
      listenerCleanup();
    }
    changeMimeHandler(oldAction[0], oldAction[1]);
    gBrowser.removeTab(tab);
  });
});

function changeMimeHandler(preferredAction, alwaysAskBeforeHandling) {
  let handlerService = Cc[
    "@mozilla.org/uriloader/handler-service;1"
  ].getService(Ci.nsIHandlerService);
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );
  var oldAction = [
    handlerInfo.preferredAction,
    handlerInfo.alwaysAskBeforeHandling,
  ];

  // Change and save mime handler settings
  handlerInfo.alwaysAskBeforeHandling = alwaysAskBeforeHandling;
  handlerInfo.preferredAction = preferredAction;
  handlerService.store(handlerInfo);

  // Refresh data
  handlerInfo = mimeService.getFromTypeAndExtension("application/pdf", "pdf");

  // Test: Mime handler was updated
  is(
    handlerInfo.alwaysAskBeforeHandling,
    alwaysAskBeforeHandling,
    "always-ask prompt change successful"
  );
  is(
    handlerInfo.preferredAction,
    preferredAction,
    "mime handler change successful"
  );

  return oldAction;
}

let listenerCleanup;
function addWindowListener(aURL, aCallback) {
  let listener = {
    onOpenWindow(aXULWindow) {
      info("window opened, waiting for focus");
      listenerCleanup();
      listenerCleanup = null;

      var domwindow = aXULWindow.docShell.domWindow;
      waitForFocus(function() {
        is(
          domwindow.document.location.href,
          aURL,
          "should have seen the right window open"
        );
        domwindow.close();
        aCallback();
      }, domwindow);
    },
    onCloseWindow(aXULWindow) {},
  };
  Services.wm.addListener(listener);
  listenerCleanup = () => Services.wm.removeListener(listener);
}
