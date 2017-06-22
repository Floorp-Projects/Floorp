function pageScript() {
  window.addEventListener("beforeunload", function (event) {
    var str = "Some text that causes the beforeunload dialog to be shown";
    event.returnValue = str;
    return str;
  }, true);
}

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

const PAGE_URL =
  "data:text/html," + encodeURIComponent("<script>(" + pageScript.toSource() + ")();</script>");

add_task(async function enableDialogs() {
  // The onbeforeunload dialog should appear.
  let dialogShown = false;
  function onDialogShown(node) {
    dialogShown = true;
    let dismissButton = node.ui.button0;
    dismissButton.click();
  }
  let obsName = "tabmodal-dialog-loaded";
  Services.obs.addObserver(onDialogShown, obsName);
  await openPage(true);
  Services.obs.removeObserver(onDialogShown, obsName);
  Assert.ok(dialogShown);
});

add_task(async function disableDialogs() {
  // The onbeforeunload dialog should NOT appear.
  await openPage(false);
  info("If we time out here, then the dialog was shown...");
});

async function openPage(enableDialogs) {
  // Open about:blank in a new tab.
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, async function(browser) {
    // Load the content script in the frame.
    let methodName = enableDialogs ? "enableDialogs" : "disableDialogs";
    await ContentTask.spawn(browser, methodName, async function(name) {
      Components.utils.import("resource://gre/modules/Services.jsm");
      Services.obs.addObserver(doc => {
        if (content && doc == content.document) {
          content.QueryInterface(Ci.nsIInterfaceRequestor).
            getInterface(Ci.nsIDOMWindowUtils)[name]();
        }
      }, "document-element-inserted");
    });
    // Load the page.
    await BrowserTestUtils.loadURI(browser, PAGE_URL);
    await BrowserTestUtils.browserLoaded(browser);
    // And then navigate away.
    await BrowserTestUtils.loadURI(browser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(browser);
  });
}
