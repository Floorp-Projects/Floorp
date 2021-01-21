"use strict";

/**
 * Helper function for testing frame crashing. Some tabs are opened
 * containing frames from example.com and then the process for
 * example.com is crashed. Notifications should apply to each tab
 * and all should close when one of the notifications is closed.
 *
 * @param numTabs the number of tabs to open.
 */
async function testFrameCrash(numTabs) {
  let browser, rootBC, iframeBC;

  for (let count = 0; count < numTabs; count++) {
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: "about:blank",
    });

    browser = tab.linkedBrowser;
    rootBC = browser.browsingContext;

    // If we load example.com in an injected subframe, we assume that this
    // will load in its own subprocess, which we can then crash.
    iframeBC = await SpecialPowers.spawn(browser, [], async () => {
      let iframe = content.document.createElement("iframe");
      iframe.setAttribute("src", "http://example.com");

      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");
      return iframe.frameLoader.browsingContext;
    });
  }

  is(iframeBC.parent, rootBC, "oop frame has root as parent");

  let eventFiredPromise = BrowserTestUtils.waitForEvent(
    browser,
    "oop-browser-crashed"
  );

  BrowserTestUtils.crashFrame(
    browser,
    true /* shouldShowTabCrashPage */,
    true /* shouldClearMinidumps */,
    iframeBC
  );

  let notificationPromise = BrowserTestUtils.waitForNotificationBar(
    gBrowser,
    browser,
    "subframe-crashed"
  );

  info("Waiting for oop-browser-crashed event.");
  await eventFiredPromise.then(event => {
    ok(!event.isTopFrame, "should not be reporting top-level frame crash");
    ok(event.childID != 0, "childID is non-zero");

    isnot(
      event.browsingContextId,
      rootBC,
      "top frame browsing context id not expected."
    );

    is(
      event.browsingContextId,
      iframeBC.id,
      "oop frame browsing context id expected."
    );
  });

  if (numTabs == 1) {
    // The BrowsingContext is re-used, but the window global might still be
    // getting set up at this point, so wait until it's been initialized.
    let {
      subject: windowGlobal,
    } = await BrowserUtils.promiseObserved("window-global-created", wgp =>
      wgp.documentURI.spec.startsWith("about:framecrashed")
    );

    is(
      windowGlobal,
      iframeBC.currentWindowGlobal,
      "Resolved on expected window global"
    );

    let newIframeURI = await SpecialPowers.spawn(iframeBC, [], async () => {
      return content.document.documentURI;
    });

    ok(
      newIframeURI.startsWith("about:framecrashed"),
      "The iframe is now pointing at about:framecrashed"
    );
  }

  // Next, check that the crash notification bar has appeared.
  await notificationPromise;

  for (let count = 1; count <= numTabs; count++) {
    let notificationBox = gBrowser.getNotificationBox(gBrowser.browsers[count]);
    let notification = notificationBox.currentNotification;
    ok(notification, "Notification " + count + " should be visible");
    is(
      notification.getAttribute("value"),
      "subframe-crashed",
      "Should be showing the right notification" + count
    );

    let buttons = notification.querySelectorAll(".notification-button");
    is(
      buttons.length,
      2,
      "Notification " + count + " should have only two buttons."
    );
  }

  // Press the ignore button on the visible notification.
  let notificationBox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
  let notification = notificationBox.currentNotification;
  notification.dismiss();

  for (let count = 1; count <= numTabs; count++) {
    let nb = gBrowser.getNotificationBox(gBrowser.browsers[count]);

    await TestUtils.waitForCondition(
      () => !nb.currentNotification,
      "notification closed"
    );

    ok(
      !nb.currentNotification,
      "notification " + count + " closed when dismiss button is pressed"
    );
  }

  for (let count = 1; count <= numTabs; count++) {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
}

/**
 * In this test, we crash an out-of-process iframe and
 * verify that :
 *  1. the "oop-browser-crashed" event is dispatched with
 *     the browsing context of the crashed oop subframe.
 *  2. the crashed subframe is now pointing at "about:framecrashed"
 *     page.
 */
add_task(async function() {
  // Open a new window with fission enabled.
  ok(
    SpecialPowers.useRemoteSubframes,
    "This test only makes sense of we can use OOP iframes."
  );

  // Create the crash reporting directory if it doesn't yet exist, otherwise, a failure
  // sometimes occurs. See bug 1687855 for fixing this.
  const uAppDataPath = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  let path = PathUtils.join(uAppDataPath, "Crash Reports", "pending");
  await IOUtils.makeDirectory(path, { ignoreExisting: true });

  // Test both one tab and when four tabs are opened.
  await testFrameCrash(1);
  await testFrameCrash(4);
});
