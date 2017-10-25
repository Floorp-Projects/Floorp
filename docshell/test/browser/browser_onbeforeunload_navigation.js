function contentTask() {
  let finish;
  let promise = new Promise(resolve => { finish = resolve; });

  let contentWindow;
  let originalLocation;
  let currentTest = -1;
  let stayingOnPage = true;

  const TEST_PAGE = "http://mochi.test:8888/browser/docshell/test/browser/file_bug1046022.html";
  const TARGETED_PAGE = "data:text/html," + encodeURIComponent("<body>Shouldn't be seeing this</body>");

  let loadExpected = TEST_PAGE;
  var testsLength;

  function onTabLoaded(event) {
    info("A document loaded in a tab!");
    let loadedPage = event.target.location.href;
    if (loadedPage == "about:blank" ||
        event.originalTarget != content.document) {
      return;
    }

    if (!loadExpected) {
      ok(false, "Expected no page loads, but loaded " + loadedPage + " instead!");
      return;
    }

    if (!testsLength) {
      testsLength = content.wrappedJSObject.testFns.length;
    }

    is(loadedPage, loadExpected, "Loaded the expected page");
    if (contentWindow) {
      is(contentWindow.document, event.target, "Same doc");
    }
    if (onAfterPageLoad) {
      onAfterPageLoad();
    }
  }

  function onAfterTargetedPageLoad() {
    ok(!stayingOnPage, "We should only fire if we're expecting to let the onbeforeunload dialog proceed to the new location");
    is(content.location.href, TARGETED_PAGE, "Should have loaded the expected new page");

    runNextTest();
  }

  function onTabModalDialogLoaded() {
    info(content.location.href);
    is(content, contentWindow, "Window should be the same still.");
    is(content.location.href, originalLocation, "Page should not have changed.");
    is(content.mySuperSpecialMark, 42, "Page should not have refreshed.");

    ok(!content.dialogWasInvoked, "Dialog should only be invoked once per test.");
    content.dialogWasInvoked = true;

    addMessageListener("test-beforeunload:dialog-gone", function listener(msg) {
      removeMessageListener(msg.name, listener);

      info("Dialog is gone");
      // If we're staying on the page, run the next test from here
      if (stayingOnPage) {
        Services.tm.dispatchToMainThread(runNextTest);
      }
      // if we accepted a page load in the dialog, the next test will get started
      // by the load handler for that page loading
    });

    // If we're going to let the page load, set us up to listen for that happening:
    if (!stayingOnPage) {
      loadExpected = TARGETED_PAGE;
      onAfterPageLoad = onAfterTargetedPageLoad;
    }

    sendAsyncMessage("test-beforeunload:dialog-response", stayingOnPage);
  }

  // Listen for the dialog being created
  addMessageListener("test-beforeunload:dialog", onTabModalDialogLoaded);

  function runNextTest() {
    currentTest++;
    if (currentTest >= testsLength) {
      if (!stayingOnPage) {
        finish();
        return;
      }
      // Run the same tests again, but this time let the navigation happen:
      stayingOnPage = false;
      // Remove onbeforeunload handler, or this load will trigger the dialog...
      contentWindow.onbeforeunload = null;
      currentTest = 0;
    }


    if (!stayingOnPage) {
      // Right now we're on the data: page. Null contentWindow out to
      // avoid CPOW errors when contentWindow is no longer the correct
      // outer window proxy object.
      contentWindow = null;

      onAfterPageLoad = runCurrentTest;
      loadExpected = TEST_PAGE;
      content.location = TEST_PAGE;
    } else {
      runCurrentTest();
    }
  }

  function runCurrentTest() {
    // Reset things so we're sure the previous tests failings don't influence this one:
    contentWindow = content;
    contentWindow.mySuperSpecialMark = 42;
    contentWindow.dialogWasInvoked = false;
    originalLocation = contentWindow.location.href;
    // And run this test:
    info("Running test with onbeforeunload " + contentWindow.wrappedJSObject.testFns[currentTest].toSource());
    contentWindow.onbeforeunload = contentWindow.wrappedJSObject.testFns[currentTest];
    sendAsyncMessage("test-beforeunload:reset");
    content.location = TARGETED_PAGE;
  }

  var onAfterPageLoad = runNextTest;

  addEventListener("load", onTabLoaded, true);

  content.location = TEST_PAGE;

  return promise.then(() => {
    // Remove the handler, or closing this tab will prove tricky:
    if (contentWindow) {
      try {
        contentWindow.onbeforeunload = null;
      } catch (ex) {}
    }
    contentWindow = null;
  });
}

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

var testTab;

var loadStarted = false;
var tabStateListener = {
  onStateChange: function(webprogress, request, stateFlags, status) {
    let startDocumentFlags = Ci.nsIWebProgressListener.STATE_START |
                             Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    if ((stateFlags & startDocumentFlags) == startDocumentFlags) {
      loadStarted = true;
    }
  },
  onStatusChange: () => {},
  onLocationChange: () => {},
  onSecurityChange: () => {},
  onProgressChange: () => {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener])
};

function onTabModalDialogLoaded(node) {
  let mm = testTab.linkedBrowser.messageManager;
  mm.sendAsyncMessage("test-beforeunload:dialog");

  if (gMultiProcessBrowser) {
    // In non-e10s, onTabModalDialogLoaded fires synchronously while
    // the test-beforeunload:reset message is sent
    // asynchronously. It's easier to simply disable this assertion in
    // non-e10s than to make everything work correctly in both
    // configurations.
    ok(!loadStarted, "No load should be started.");
  }

  // Now listen for the dialog going away again...
  let observer = new MutationObserver(function(muts) {
    if (!node.parentNode) {
      observer.disconnect();
      observer = null;

      Services.tm.dispatchToMainThread(() => {
        mm.sendAsyncMessage("test-beforeunload:dialog-gone");
      });
    }
  });
  observer.observe(node.parentNode, {childList: true});

  BrowserTestUtils.waitForMessage(mm, "test-beforeunload:dialog-response").then((stayingOnPage) => {
    let button = stayingOnPage ? node.ui.button1 : node.ui.button0;
    // ... and then actually make the dialog go away
    info("Clicking button: " + button.label);
    EventUtils.synthesizeMouseAtCenter(button, {});
  });
}

// Listen for the dialog being created
Services.obs.addObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");

function test() {
  waitForExplicitFinish();
  gBrowser.addProgressListener(tabStateListener);

  testTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  testTab.linkedBrowser.messageManager.addMessageListener("test-beforeunload:reset", () => {
    loadStarted = false;
  });

  ContentTask.spawn(testTab.linkedBrowser, null, contentTask).then(finish);
}

registerCleanupFunction(function() {
  Services.obs.removeObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
  gBrowser.removeProgressListener(tabStateListener);
  gBrowser.removeTab(testTab);
});

