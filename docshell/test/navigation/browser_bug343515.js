// Test for bug 343515 - Need API for tabbrowsers to tell docshells they're visible/hidden

// Globals
var testPath = "http://mochi.test:8888/browser/docshell/test/navigation/";
var ctx = {};

add_task(async function () {
  // Step 1.

  // Get a handle on the initial tab
  ctx.tab0 = gBrowser.selectedTab;
  ctx.tab0Browser = gBrowser.getBrowserForTab(ctx.tab0);

  await BrowserTestUtils.waitForCondition(
    () => ctx.tab0Browser.docShellIsActive,
    "Timed out waiting for initial tab to be active."
  );

  // Open a New Tab
  ctx.tab1 = BrowserTestUtils.addTab(gBrowser, testPath + "bug343515_pg1.html");
  ctx.tab1Browser = gBrowser.getBrowserForTab(ctx.tab1);
  await BrowserTestUtils.browserLoaded(ctx.tab1Browser);

  // Step 2.
  is(
    testPath + "bug343515_pg1.html",
    ctx.tab1Browser.currentURI.spec,
    "Got expected tab 1 url in step 2"
  );

  // Our current tab should still be active
  ok(ctx.tab0Browser.docShellIsActive, "Tab 0 should still be active");
  ok(!ctx.tab1Browser.docShellIsActive, "Tab 1 should not be active");

  // Switch to tab 1
  await BrowserTestUtils.switchTab(gBrowser, ctx.tab1);

  // Tab 1 should now be active
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");

  // Open another tab
  ctx.tab2 = BrowserTestUtils.addTab(gBrowser, testPath + "bug343515_pg2.html");
  ctx.tab2Browser = gBrowser.getBrowserForTab(ctx.tab2);

  await BrowserTestUtils.browserLoaded(ctx.tab2Browser);

  // Step 3.
  is(
    testPath + "bug343515_pg2.html",
    ctx.tab2Browser.currentURI.spec,
    "Got expected tab 2 url in step 3"
  );

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");

  // Tab 2's window _and_ its iframes should be inactive
  ok(!ctx.tab2Browser.docShellIsActive, "Tab 2 should be inactive");

  await SpecialPowers.spawn(ctx.tab2Browser, [], async function () {
    Assert.equal(content.frames.length, 2, "Tab 2 should have 2 iframes");
    for (var i = 0; i < content.frames.length; i++) {
      info("step 3, frame " + i + " info: " + content.frames[i].location);
      let bc = content.frames[i].browsingContext;
      Assert.ok(!bc.isActive, `Tab2 iframe ${i} should be inactive`);
    }
  });

  // Navigate tab 2 to a different page
  BrowserTestUtils.startLoadingURIString(
    ctx.tab2Browser,
    testPath + "bug343515_pg3.html"
  );

  await BrowserTestUtils.browserLoaded(ctx.tab2Browser);

  // Step 4.

  async function checkTab2Active(outerExpected) {
    await SpecialPowers.spawn(
      ctx.tab2Browser,
      [outerExpected],
      async function (expected) {
        function isActive(aWindow) {
          var docshell = aWindow.docShell;
          info(`checking ${docshell.browsingContext.id}`);
          return docshell.browsingContext.isActive;
        }

        let active = expected ? "active" : "inactive";
        Assert.equal(content.frames.length, 2, "Tab 2 should have 2 iframes");
        for (var i = 0; i < content.frames.length; i++) {
          info("step 4, frame " + i + " info: " + content.frames[i].location);
        }
        Assert.equal(
          content.frames[0].frames.length,
          1,
          "Tab 2 iframe 0 should have 1 iframes"
        );
        Assert.equal(
          isActive(content.frames[0]),
          expected,
          `Tab2 iframe 0 should be ${active}`
        );
        Assert.equal(
          isActive(content.frames[0].frames[0]),
          expected,
          `Tab2 iframe 0 subiframe 0 should be ${active}`
        );
        Assert.equal(
          isActive(content.frames[1]),
          expected,
          `Tab2 iframe 1 should be ${active}`
        );
      }
    );
  }

  is(
    testPath + "bug343515_pg3.html",
    ctx.tab2Browser.currentURI.spec,
    "Got expected tab 2 url in step 4"
  );

  // Tab 0 should be inactive, Tab 1 should be active
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");

  // Tab2 and all descendants should be inactive
  await checkTab2Active(false);

  // Switch to Tab 2
  await BrowserTestUtils.switchTab(gBrowser, ctx.tab2);

  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(!ctx.tab1Browser.docShellIsActive, "Tab 1 should be inactive");
  ok(ctx.tab2Browser.docShellIsActive, "Tab 2 should be active");

  await checkTab2Active(true);

  // Go back
  let backDone = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    testPath + "bug343515_pg2.html"
  );
  ctx.tab2Browser.goBack();
  await backDone;

  // Step 5.

  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(!ctx.tab1Browser.docShellIsActive, "Tab 1 should be inactive");
  ok(ctx.tab2Browser.docShellIsActive, "Tab 2 should be active");
  is(
    testPath + "bug343515_pg2.html",
    ctx.tab2Browser.currentURI.spec,
    "Got expected tab 2 url in step 5"
  );

  await SpecialPowers.spawn(ctx.tab2Browser, [], async function () {
    for (var i = 0; i < content.frames.length; i++) {
      let bc = content.frames[i].browsingContext;
      Assert.ok(bc.isActive, `Tab2 iframe ${i} should be active`);
    }
  });

  // Switch to tab 1
  await BrowserTestUtils.switchTab(gBrowser, ctx.tab1);

  // Navigate to page 3
  BrowserTestUtils.startLoadingURIString(
    ctx.tab1Browser,
    testPath + "bug343515_pg3.html"
  );

  await BrowserTestUtils.browserLoaded(ctx.tab1Browser);

  // Step 6.

  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");
  is(
    testPath + "bug343515_pg3.html",
    ctx.tab1Browser.currentURI.spec,
    "Got expected tab 1 url in step 6"
  );

  await SpecialPowers.spawn(ctx.tab1Browser, [], async function () {
    function isActive(aWindow) {
      var docshell = aWindow.docShell;
      info(`checking ${docshell.browsingContext.id}`);
      return docshell.browsingContext.isActive;
    }

    Assert.ok(isActive(content.frames[0]), "Tab1 iframe 0 should be active");
    Assert.ok(
      isActive(content.frames[0].frames[0]),
      "Tab1 iframe 0 subiframe 0 should be active"
    );
    Assert.ok(isActive(content.frames[1]), "Tab1 iframe 1 should be active");
  });

  ok(!ctx.tab2Browser.docShellIsActive, "Tab 2 should be inactive");

  await SpecialPowers.spawn(ctx.tab2Browser, [], async function () {
    for (var i = 0; i < content.frames.length; i++) {
      let bc = content.frames[i].browsingContext;
      Assert.ok(!bc.isActive, `Tab2 iframe ${i} should be inactive`);
    }
  });

  // Go forward on tab 2
  let forwardDone = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    testPath + "bug343515_pg3.html"
  );
  ctx.tab2Browser.goForward();
  await forwardDone;

  // Step 7.

  async function checkBrowser(browser, outerTabNum, outerActive) {
    let data = { tabNum: outerTabNum, active: outerActive };
    await SpecialPowers.spawn(
      browser,
      [data],
      async function ({ tabNum, active }) {
        function isActive(aWindow) {
          var docshell = aWindow.docShell;
          info(`checking ${docshell.browsingContext.id}`);
          return docshell.browsingContext.isActive;
        }

        let activestr = active ? "active" : "inactive";
        Assert.equal(
          isActive(content.frames[0]),
          active,
          `Tab${tabNum} iframe 0 should be ${activestr}`
        );
        Assert.equal(
          isActive(content.frames[0].frames[0]),
          active,
          `Tab${tabNum} iframe 0 subiframe 0 should be ${activestr}`
        );
        Assert.equal(
          isActive(content.frames[1]),
          active,
          `Tab${tabNum} iframe 1 should be ${activestr}`
        );
      }
    );
  }

  // Check everything
  ok(!ctx.tab0Browser.docShellIsActive, "Tab 0 should be inactive");
  ok(ctx.tab1Browser.docShellIsActive, "Tab 1 should be active");
  is(
    testPath + "bug343515_pg3.html",
    ctx.tab2Browser.currentURI.spec,
    "Got expected tab 2 url in step 7"
  );

  await checkBrowser(ctx.tab1Browser, 1, true);

  ok(!ctx.tab2Browser.docShellIsActive, "Tab 2 should be inactive");
  await checkBrowser(ctx.tab2Browser, 2, false);

  // Close the tabs we made
  BrowserTestUtils.removeTab(ctx.tab1);
  BrowserTestUtils.removeTab(ctx.tab2);
});
