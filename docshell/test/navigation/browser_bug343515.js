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

// We need to wait until the page from each testcase is fully loaded,
// including all of its descendant iframes. To do that we manually count
// how many load events should happen on that page (one for the toplevel doc
// and one for each subframe) and wait until we receive the expected number
// of events.
function nShotsListener(aElem, aType, aCallback, aCount) {
  let count = aCount;
  aElem.addEventListener(aType, function listenerCallback() {
    if (--count == 0) {
      aElem.removeEventListener(aType, listenerCallback, true);

      // aCallback is executed asynchronously, which is handy because load
      // events fire before mIsDocumentLoaded is actually set to true. :(
      executeSoon(aCallback);
    }
  }, true);
}

function oneShotListener(aElem, aType, aCallback) {
  nShotsListener(aElem, aType, aCallback, 1);
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
  is(testPath + "bug343515_pg1.html", ctx.tab1Browser.currentURI.spec,
     "Got expected tab 1 url in step 2");

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

  // bug343515_pg2.html consists of a page with two iframes,
  // which will therefore generate 3 load events.
  nShotsListener(ctx.tab2Browser, "load", step3, 3);
}

function step3() {
  is(testPath + "bug343515_pg2.html", ctx.tab2Browser.currentURI.spec,
     "Got expected tab 2 url in step 3");

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");

  // Tab 2's window _and_ its iframes should be inactive
  ok(!isActive(ctx.tab2Window), "Tab 2 should be inactive");
  is(ctx.tab2Window.frames.length, 2, "Tab 2 should have 2 iframes");
  for (var i = 0; i < ctx.tab2Window.frames.length; i++)
    info("step 3, frame " + i + " info: " + ctx.tab2Window.frames[i].location);
  ok(!isActive(ctx.tab2Window.frames[0]), "Tab2 iframe 0 should be inactive");
  ok(!isActive(ctx.tab2Window.frames[1]), "Tab2 iframe 1 should be inactive");

  // Navigate tab 2 to a different page
  ctx.tab2Window.location = testPath + "bug343515_pg3.html";

  // bug343515_pg3.html consists of a page with two iframes, one of which
  // contains another iframe, so there'll be a total of 4 load events
  nShotsListener(ctx.tab2Browser, "load", step4, 4);
}

function step4() {
  is(testPath + "bug343515_pg3.html", ctx.tab2Browser.currentURI.spec,
     "Got expected tab 2 url in step 4");

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!isActive(ctx.tab0Window), "Tab 0 should be inactive");
  ok(isActive(ctx.tab1Window), "Tab 1 should be active");

  // Tab2 and all descendants should be inactive
  ok(!isActive(ctx.tab2Window), "Tab 2 should be inactive");
  is(ctx.tab2Window.frames.length, 2, "Tab 2 should have 2 iframes");
  for (var i = 0; i < ctx.tab2Window.frames.length; i++)
    info("step 4, frame " + i + " info: " + ctx.tab2Window.frames[i].location);
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
  oneShotListener(ctx.tab2Browser, "pageshow", step5);
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

  // bug343515_pg3.html consists of a page with two iframes, one of which
  // contains another iframe, so there'll be a total of 4 load events
  nShotsListener(ctx.tab1Browser, "load", step6, 4);
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
  oneShotListener(ctx.tab2Browser, "pageshow", step7);
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
