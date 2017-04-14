function pageScript() {
  window.addEventListener("beforeunload", function (event) {
    var str = "Some text that causes the beforeunload dialog to be shown";
    event.returnValue = str;
    return str;
  }, true);
}

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", true]]});

const PAGE_URL =
  "data:text/html," + encodeURIComponent("<script>(" + pageScript.toSource() + ")();</script>");

add_task(function* doClick() {
  // The onbeforeunload dialog should appear.
  let dialogShown = false;
  function onDialogShown(node) {
    dialogShown = true;
    let dismissButton = node.ui.button0;
    dismissButton.click();
  }
  let obsName = "tabmodal-dialog-loaded";
  Services.obs.addObserver(onDialogShown, obsName);
  yield* openPage(true);
  Services.obs.removeObserver(onDialogShown, obsName);
  Assert.ok(dialogShown, "Should have shown dialog.");
});

add_task(function* noClick() {
  // The onbeforeunload dialog should NOT appear.
  yield openPage(false);
  info("If we time out here, then the dialog was shown...");
});

function* openPage(shouldClick) {
  // Open about:blank in a new tab.
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
    // Load the page.
    yield BrowserTestUtils.loadURI(browser, PAGE_URL);
    yield BrowserTestUtils.browserLoaded(browser);

    if (shouldClick) {
      yield BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);
    }
    let hasInteractedWith = yield ContentTask.spawn(browser, "", function() {
      return content.document.userHasInteracted;
    });
    is(shouldClick, hasInteractedWith, "Click should update document interactivity state");
    // And then navigate away.
    yield BrowserTestUtils.loadURI(browser, "http://example.com/");
    yield BrowserTestUtils.browserLoaded(browser);
  });
}
