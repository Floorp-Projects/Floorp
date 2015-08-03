function pageScript() {
  window.addEventListener("beforeunload", function (event) {
    var str = "Some text that causes the beforeunload dialog to be shown";
    event.returnValue = str;
    return str;
  }, true);
}

const PAGE_URL =
  "data:text/html," + encodeURIComponent("<script>(" + pageScript.toSource() + ")();</script>");

add_task(function* enableDialogs() {
  // The onbeforeunload dialog should appear.
  let dialogShown = false;
  function onDialogShown(node) {
    dialogShown = true;
    let dismissButton = node.ui.button0;
    dismissButton.click();
  }
  let obsName = "tabmodal-dialog-loaded";
  Services.obs.addObserver(onDialogShown, obsName, false);
  yield openPage(true);
  Services.obs.removeObserver(onDialogShown, obsName);
  Assert.ok(dialogShown);
});

add_task(function* disableDialogs() {
  // The onbeforeunload dialog should NOT appear.
  yield openPage(false);
  info("If we time out here, then the dialog was shown...");
});

function* openPage(enableDialogs) {
  // Open about:blank in a new tab.
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
    // Load the content script in the frame.
    let methodName = enableDialogs ? "enableDialogs" : "disableDialogs";
    yield ContentTask.spawn(browser, methodName, function* (name) {
      Components.utils.import("resource://gre/modules/Services.jsm");
      Services.obs.addObserver(doc => {
        if (content && doc == content.document) {
          content.QueryInterface(Ci.nsIInterfaceRequestor).
            getInterface(Ci.nsIDOMWindowUtils)[name]();
        }
      }, "document-element-inserted", false);
    });
    // Load the page.
    yield BrowserTestUtils.loadURI(browser, PAGE_URL);
    yield BrowserTestUtils.browserLoaded(browser);
    // And then navigate away.
    yield BrowserTestUtils.loadURI(browser, "http://example.com/");
    yield BrowserTestUtils.browserLoaded(browser);
  });
}
