// Test for bug 343515 - Need API for tabbrowsers to tell docshells they're visible/hidden

// Globals
var testPath = "http://mochi.test:8888/browser/docshell/test/navigation/";
var ctx = {};

// Helper function to check if a window is active
function isActive(aWindow) {
  var docshell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);
  return docshell.isActive;
}

function oneShotListener(aElem, aType, aCallback) {
  aElem.addEventListener(aType, function () {
    aElem.removeEventListener(aType, arguments.callee, true);

    // aCallback is executed asynchronously, which is handy because load
    // events fire before mIsDocumentLoaded is actually set to true. :(
    executeSoon(aCallback);
  }, true);
}

// Returns a closure that iteratively (BFS) waits for all
// of the descendant frames of aInitialWindow to finish loading,
// then calls aFinalCallback.
function frameLoadWaiter(aInitialWindow, aFinalCallback) {

  // The window we're currently waiting on
  var curr = aInitialWindow;

  // The windows we need to wait for
  var waitQueue = [];

  // The callback to call when we're all done
  var finalCallback = aFinalCallback;

  function frameLoadCallback() {

    // Push any subframes of what we just got
    for (var i = 0; i < curr.frames.length; ++i)
      waitQueue.push(curr.frames[i]);

    // Handle the next window in the queue
    if (waitQueue.length >= 1) {
      curr = waitQueue.shift();
      if (curr.document.readyState == "complete")
        frameLoadCallback();
      else
        oneShotListener(curr, "load", frameLoadCallback);
      return;
    }

    // Otherwise, we're all done. Call the final callback
    finalCallback();
  }

  return frameLoadCallback;
}

// Entry point from Mochikit
function test() {

  // Lots of callbacks going on here
  waitForExplicitFinish();

  // Begin the test
  step1();
}

function step1() {

  // Get a handle on the initial tab
  ctx.tab0 = gBrowser.selectedTab;
  ctx.tab0Browser = gBrowser.getBrowserForTab(ctx.tab0);
  ctx.tab0Window = ctx.tab0Browser.contentWindow;

  // Our current tab should be active
  ok(isActive(ctx.tab0Window), "Tab 0 should be active at test start");

  // Open a New Tab
  ctx.tab1 = gBrowser.addTab(testPath + "bug343515_pg1.html");
  ctx.tab1Browser = gBrowser.getBrowserForTab(ctx.tab1);
  ctx.tab1Window = ctx.tab1Browser.contentWindow;
  oneShotListener(ctx.tab1Browser, "load", step2);
}

function step2() {

  // Our current tab should still be active
  ok(isActive(ctx.tab0Window), "Tab 0 should still be active");
  ok(!isActive(ctx.tab1Window), "Tab 1 should not be active");

  // Switch to tab 1
  gBrowser.selectedTab = ctx.tab1;

  // Tab 1 should now be active
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");

  // Open another tab
  ctx.tab2 = gBrowser.addTab(testPath + "bug343515_pg2.html");
  ctx.tab2Browser = gBrowser.getBrowserForTab(ctx.tab2);
  ctx.tab2Window = ctx.tab2Browser.contentWindow;
  oneShotListener(ctx.tab2Browser, "load", frameLoadWaiter(ctx.tab2Window, step3));
}

function step3() {

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");

  // Tab 2's window _and_ its iframes should be inactive
  ok(!isActive(ctx.tab2Window), "Tab 2 should be inactive");
  is(ctx.tab2Window.frames.length, 2, "Tab 2 should have 2 iframes");
  ok(!isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be inactive");

  // Navigate tab 2 to a different page
  ctx.tab2Window.location = testPath + "bug343515_pg3.html";
  oneShotListener(ctx.tab2Browser, "load", frameLoadWaiter(ctx.tab2Window, step4));
}

function step4() {

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");

  // Tab2 and all descendants should be inactive
  ok(!isActive(ctx.tab2Window), "Tab 2 should be inactive");
  is(ctx.tab2Window.frames.length, 2, "Tab 2 should have 2 iframes");
  is(ctx.tab2Window.frames[0].frames.length, 1, "Tab 2 iframe 0 should have 1 iframes");
  ok(!isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[0].frames[0]), "Tab2 iframe 0 subiframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be inactive");

  // Switch to Tab 2
  gBrowser.selectedTab = ctx.tab2;

  // Check everything
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(!isActive(ctx.tab1Window), "Tab 1 should be inactive");
  ok(isActive(ctx.tab2Window), "Tab 2 should be active");
  ok(isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be active");
  ok(isActive(ctx.tab2Window.frames[0].frames[0]), "Tab2 iframe 0 subiframe 0 should be active");
  ok(isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be active");

  // Go back
  oneShotListener(ctx.tab2Browser, "pageshow", frameLoadWaiter(ctx.tab2Window, step5));
  ctx.tab2Browser.goBack();

}

function step5() {

  // Check everything
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(!isActive(ctx.tab1Window), "Tab 1 should be inactive");
  ok(isActive(ctx.tab2Window), "Tab 2 should be active");
  ok(isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be active");
  ok(isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be active");

  // Switch to tab 1
  gBrowser.selectedTab = ctx.tab1;

  // Navigate to page 3
  ctx.tab1Window.location = testPath + "bug343515_pg3.html";
  oneShotListener(ctx.tab1Browser, "load", frameLoadWaiter(ctx.tab1Window, step6));
}

function step6() {

  // Check everything
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");
  ok(isActive(ctx.tab1Window.frames[0]), "Tab1 iframe 0 should be active");
  ok(isActive(ctx.tab1Window.frames[0].frames[0]), "Tab1 iframe 0 subiframe 0 should be active");
  ok(isActive(ctx.tab1Window.frames[1]), "Tab1 iframe 1 should be active");
  ok(!isActive(ctx.tab2Window), "Tab 2 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be inactive");

  // Go forward on tab 2
  oneShotListener(ctx.tab2Browser, "pageshow", frameLoadWaiter(ctx.tab2Window, step7));
  var tab2docshell = ctx.tab2Window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIWebNavigation);
  tab2docshell.goForward();
}

function step7() {

  ctx.tab2Window = ctx.tab2Browser.contentWindow;

  // Check everything
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");
  ok(isActive(ctx.tab1Window.frames[0]), "Tab1 iframe 0 should be active");
  ok(isActive(ctx.tab1Window.frames[0].frames[0]), "Tab1 iframe 0 subiframe 0 should be active");
  ok(isActive(ctx.tab1Window.frames[1]), "Tab1 iframe 1 should be active");
  ok(!isActive(ctx.tab2Window), "Tab 2 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[0].frames[0]), "Tab2 iframe 0 subiframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be inactive");

  // That's probably enough
  allDone();
}

function allDone() {

  // Close the tabs we made
  gBrowser.removeTab(ctx.tab1);
  gBrowser.removeTab(ctx.tab2);

  // Tell the framework we're done
  finish();
}
