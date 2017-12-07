// Test for bug 343515 - Need API for tabbrowsers to tell docshells they're visible/hidden

// Globals
var testPath = "http://mochi.test:8888/browser/docshell/test/navigation/";
var ctx = {};

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

function waitForPageshow(aBrowser, callback) {
  return ContentTask.spawn(aBrowser, null, async function() {
    await ContentTaskUtils.waitForEvent(this, "pageshow");
  }).then(callback);
}

// Entry point from Mochikit
function test() {

  // Lots of callbacks going on here
  waitForExplicitFinish();

  // Begin the test. First, we disable tab warming because it's
  // possible for the mouse to be over one of the tabs during
  // this test, warm it up, and cause this test to fail intermittently
  // in automation.
  SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.remote.warmup.enabled", false]],
  }).then(step1);
}

function step1() {

  // Get a handle on the initial tab
  ctx.tab0 = gBrowser.selectedTab;
  ctx.tab0Browser = gBrowser.getBrowserForTab(ctx.tab0);

  // Our current tab should be active
  ok(ctx.tab0Browser.docShellIsActive, "Tab 0 should be active at test start");

  // Open a New Tab
  ctx.tab1 = BrowserTestUtils.addTab(gBrowser, testPath + "bug343515_pg1.html");
  ctx.tab1Browser = gBrowser.getBrowserForTab(ctx.tab1);
  oneShotListener(ctx.tab1Browser, "load", step2);
}

function step2() {
  is(testPath + "bug343515_pg1.html", ctx.tab1Browser.currentURI.spec,
     "Got expected tab 1 url in step 2");

  // Our current tab should still be active
  ok(ctx.tab0Browser.docShellIsActive, "Tab 0 should still be active");
  ok(!ctx.tab1Browser.docShellIsActive, "Tab 1 should not be active");

  // Switch to tab 1
  BrowserTestUtils.switchTab(gBrowser, ctx.tab1).then(() => {
    // Tab 1 should now be active
    ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
    ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");

    // Open another tab
    ctx.tab2 = BrowserTestUtils.addTab(gBrowser, testPath + "bug343515_pg2.html");
    ctx.tab2Browser = gBrowser.getBrowserForTab(ctx.tab2);

    // bug343515_pg2.html consists of a page with two iframes,
    // which will therefore generate 3 load events.
    nShotsListener(ctx.tab2Browser, "load", step3, 3);
  });
}

function step3() {
  is(testPath + "bug343515_pg2.html", ctx.tab2Browser.currentURI.spec,
     "Got expected tab 2 url in step 3");

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");

  // Tab 2's window _and_ its iframes should be inactive
  ok(!ctx.tab2Browser.docShellIsActive, "Tab 2 should be inactive");
  ContentTask.spawn(ctx.tab2Browser, null, async function() {
    Assert.equal(content.frames.length, 2, "Tab 2 should have 2 iframes");
    for (var i = 0; i < content.frames.length; i++) {
      info("step 3, frame " + i + " info: " + content.frames[i].location);
      let docshell = content.frames[i].QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIWebNavigation)
                                      .QueryInterface(Ci.nsIDocShell);

      Assert.ok(!docShell.isActive, `Tab2 iframe ${i} should be inactive`);
    }
  }).then(() => {
    // Navigate tab 2 to a different page
    ctx.tab2Browser.loadURI(testPath + "bug343515_pg3.html");

    // bug343515_pg3.html consists of a page with two iframes, one of which
    // contains another iframe, so there'll be a total of 4 load events
    nShotsListener(ctx.tab2Browser, "load", step4, 4);
  });
}

function step4() {
  function checkTab2Active(expected) {
    return ContentTask.spawn(ctx.tab2Browser, expected, async function(expected) {
      function isActive(aWindow) {
        var docshell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebNavigation)
                              .QueryInterface(Ci.nsIDocShell);
        return docshell.isActive;
      }

      let active = expected ? "active" : "inactive";
      Assert.equal(content.frames.length, 2, "Tab 2 should have 2 iframes");
      for (var i = 0; i < content.frames.length; i++)
        info("step 4, frame " + i + " info: " + content.frames[i].location);
      Assert.equal(content.frames[0].frames.length, 1, "Tab 2 iframe 0 should have 1 iframes");
      Assert.equal(isActive(content.frames[0]), expected, `Tab2 iframe 0 should be ${active}`);
      Assert.equal(isActive(content.frames[0].frames[0]), expected,
         `Tab2 iframe 0 subiframe 0 should be ${active}`);
      Assert.equal(isActive(content.frames[1]), expected, `Tab2 iframe 1 should be ${active}`);
    });
  }

  is(testPath + "bug343515_pg3.html", ctx.tab2Browser.currentURI.spec,
     "Got expected tab 2 url in step 4");

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");

  // Tab2 and all descendants should be inactive
  checkTab2Active(false).then(() => {
    // Switch to Tab 2
    return BrowserTestUtils.switchTab(gBrowser, ctx.tab2);
  }).then(() => {
    // Check everything
    ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
    ok(!ctx.tab1Browser.docShellIsActive, "Tab 1 should be inactive");
    ok(ctx.tab2Browser.docShellIsActive, "Tab 2 should be active");

    return checkTab2Active(true);
  }).then(() => {
    // Go back
    waitForPageshow(ctx.tab2Browser, step5);
    ctx.tab2Browser.goBack();
  });
}

function step5() {
  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(!ctx.tab1Browser.docShellIsActive, "Tab 1 should be inactive");
  ok(ctx.tab2Browser.docShellIsActive, "Tab 2 should be active");
  ContentTask.spawn(ctx.tab2Browser, null, async function() {
    for (var i = 0; i < content.frames.length; i++) {
      let docshell = content.frames[i].QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIWebNavigation)
                                      .QueryInterface(Ci.nsIDocShell);

      Assert.ok(docShell.isActive, `Tab2 iframe ${i} should be active`);
    }
  }).then(() => {
    // Switch to tab 1
    return BrowserTestUtils.switchTab(gBrowser, ctx.tab1);
  }).then(() => {
    // Navigate to page 3
    ctx.tab1Browser.loadURI(testPath + "bug343515_pg3.html");

    // bug343515_pg3.html consists of a page with two iframes, one of which
    // contains another iframe, so there'll be a total of 4 load events
    nShotsListener(ctx.tab1Browser, "load", step6, 4);
  });
}

function step6() {

  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");
  ContentTask.spawn(ctx.tab1Browser, null, async function() {
    function isActive(aWindow) {
      var docshell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIDocShell);
      return docshell.isActive;
    }

    Assert.ok(isActive(content.frames[0]), "Tab1 iframe 0 should be active");
    Assert.ok(isActive(content.frames[0].frames[0]), "Tab1 iframe 0 subiframe 0 should be active");
    Assert.ok(isActive(content.frames[1]), "Tab1 iframe 1 should be active");
  }).then(() => {
    ok(!ctx.tab2Browser.docShellIsActive, "Tab 2 should be inactive");
    return ContentTask.spawn(ctx.tab2Browser, null, async function() {
      for (var i = 0; i < content.frames.length; i++) {
        let docshell = content.frames[i].QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIWebNavigation)
                                        .QueryInterface(Ci.nsIDocShell);

        Assert.ok(!docShell.isActive, `Tab2 iframe ${i} should be inactive`);
      }
    });
  }).then(() => {
    // Go forward on tab 2
    waitForPageshow(ctx.tab2Browser, step7);
    ctx.tab2Browser.goForward();
  });
}

function step7() {
  function checkBrowser(browser, tabNum, active) {
    return ContentTask.spawn(browser, { tabNum, active },
                             async function({ tabNum, active }) {
             function isActive(aWindow) {
               var docshell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                     .getInterface(Ci.nsIWebNavigation)
                                     .QueryInterface(Ci.nsIDocShell);
               return docshell.isActive;
             }

             let activestr = active ? "active" : "inactive";
             Assert.equal(isActive(content.frames[0]), active,
                `Tab${tabNum} iframe 0 should be ${activestr}`);
             Assert.equal(isActive(content.frames[0].frames[0]), active,
                `Tab${tabNum} iframe 0 subiframe 0 should be ${activestr}`);
             Assert.equal(isActive(content.frames[1]), active,
                `Tab${tabNum} iframe 1 should be ${activestr}`);
           });
  }

  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");
  checkBrowser(ctx.tab1Browser, 1, true).then(() => {
    ok(!ctx.tab2Browser.docShellIsActive, "Tab 2 should be inactive");
    return checkBrowser(ctx.tab2Browser, 2, false);
  }).then(() => {
    // That's probably enough
    allDone();
  });
}

function allDone() {

  // Close the tabs we made
  gBrowser.removeTab(ctx.tab1);
  gBrowser.removeTab(ctx.tab2);

  // Tell the framework we're done
  finish();
}
