var contentWindow;
var originalLocation;
var currentTest = -1;
var stayingOnPage = true;

var TEST_PAGE = "http://mochi.test:8888/browser/docshell/test/browser/file_bug1046022.html";
var TARGETED_PAGE = "data:text/html," + encodeURIComponent("<body>Shouldn't be seeing this</body>");

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

var loadExpected = TEST_PAGE;
var testTab;
var testsLength;

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

function onTabLoaded(event) {
  info("A document loaded in a tab!");
  let loadedPage = event.target.location.href;
  if (loadedPage == "about:blank" ||
      event.originalTarget != testTab.linkedBrowser.contentDocument) {
    return;
  }

  if (!loadExpected) {
    ok(false, "Expected no page loads, but loaded " + loadedPage + " instead!");
    return;
  }

  if (!testsLength) {
    testsLength = testTab.linkedBrowser.contentWindow.wrappedJSObject.testFns.length;
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
  is(testTab.linkedBrowser.currentURI.spec, TARGETED_PAGE, "Should have loaded the expected new page");

  runNextTest();
}

function onTabModalDialogLoaded(node) {
  let content = testTab.linkedBrowser.contentWindow;
  ok(!loadStarted, "No load should be started.");
  info(content.location.href);
  is(content, contentWindow, "Window should be the same still.");
  is(content.location.href, originalLocation, "Page should not have changed.");
  is(content.mySuperSpecialMark, 42, "Page should not have refreshed.");

  ok(!content.dialogWasInvoked, "Dialog should only be invoked once per test.");
  content.dialogWasInvoked = true;


  // Now listen for the dialog going away again...
  let observer = new MutationObserver(function(muts) {
    if (!node.parentNode) {
      info("Dialog is gone");
      observer.disconnect();
      observer = null;
      // If we're staying on the page, run the next test from here
      if (stayingOnPage) {
        // Evil, but necessary: without this delay, we manage to still break our
        // own onbeforeunload code, because we'll basically cause a new load to be
        // started while processing the destruction of the dialog for the old one.
        executeSoon(runNextTest);
      }
      // if we accepted a page load in the dialog, the next test will get started
      // by the load handler for that page loading
    }
  });
  observer.observe(node.parentNode, {childList: true});

  // If we're going to let the page load, set us up to listen for that happening:
  if (!stayingOnPage) {
    loadExpected = TARGETED_PAGE;
    onAfterPageLoad = onAfterTargetedPageLoad;
  }

  let button = stayingOnPage ? node.ui.button1 : node.ui.button0;
  // ... and then actually make the dialog go away
  info("Clicking button: " + button.label);
  EventUtils.synthesizeMouseAtCenter(button, {});
}

// Listen for the dialog being created
Services.obs.addObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");

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
    testTab.linkedBrowser.loadURI(TEST_PAGE);
  } else {
    runCurrentTest();
  }
}

function runCurrentTest() {
  // Reset things so we're sure the previous tests failings don't influence this one:
  contentWindow = testTab.linkedBrowser.contentWindow;
  contentWindow.mySuperSpecialMark = 42;
  contentWindow.dialogWasInvoked = false;
  originalLocation = contentWindow.location.href;
  // And run this test:
  info("Running test with onbeforeunload " + contentWindow.wrappedJSObject.testFns[currentTest].toSource());
  contentWindow.onbeforeunload = contentWindow.wrappedJSObject.testFns[currentTest];
  loadStarted = false;
  testTab.linkedBrowser.loadURI(TARGETED_PAGE);
}

var onAfterPageLoad = runNextTest;

function test() {
  waitForExplicitFinish();
  gBrowser.addProgressListener(tabStateListener);

  testTab = gBrowser.selectedTab = gBrowser.addTab();
  testTab.linkedBrowser.addEventListener("load", onTabLoaded, true);
  testTab.linkedBrowser.loadURI(TEST_PAGE);
}

registerCleanupFunction(function() {
  // Remove the handler, or closing this tab will prove tricky:
  if (contentWindow) {
    try {
      contentWindow.onbeforeunload = null;
    } catch (ex) {}
  }
  contentWindow = null;
  testTab.linkedBrowser.removeEventListener("load", onTabLoaded, true);
  Services.obs.removeObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
  gBrowser.removeProgressListener(tabStateListener);
  gBrowser.removeTab(testTab);
});

