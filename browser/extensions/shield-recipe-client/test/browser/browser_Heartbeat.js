"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://shield-recipe-client/lib/Heartbeat.jsm", this);
Cu.import("resource://shield-recipe-client/lib/SandboxManager.jsm", this);

/**
 * Assert an array is in non-descending order, and that every element is a number
 */
function assertOrdered(arr) {
  for (let i = 0; i < arr.length; i++) {
    Assert.equal(typeof arr[i], "number", `element ${i} is type "number"`);
  }
  for (let i = 0; i < arr.length - 1; i++) {
    Assert.lessOrEqual(arr[i], arr[i + 1],
      `element ${i} is less than or equal to element ${i + 1}`);
  }
}

/* Close every notification in a target window and notification box */
function closeAllNotifications(targetWindow, notificationBox) {
  if (notificationBox.allNotifications.length === 0) {
    return Promise.resolve();
  }


  return new Promise(resolve => {
    const notificationSet = new Set(notificationBox.allNotifications);

    const observer = new targetWindow.MutationObserver(mutations => {
      for (const mutation of mutations) {
        for (let i = 0; i < mutation.removedNodes.length; i++) {
          const node = mutation.removedNodes.item(i);
          if (notificationSet.has(node)) {
            notificationSet.delete(node);
          }
        }
      }
      if (notificationSet.size === 0) {
        Assert.equal(notificationBox.allNotifications.length, 0, "No notifications left");
        observer.disconnect();
        resolve();
      }
    });

    observer.observe(notificationBox, {childList: true});

    for (const notification of notificationBox.allNotifications) {
      notification.close();
    }
  });
}

/* Check that the correct telemetry was sent */
function assertTelemetrySent(hb, eventNames) {
  return new Promise(resolve => {
    hb.eventEmitter.once("TelemetrySent", payload => {
      const events = [0];
      for (const name of eventNames) {
        Assert.equal(typeof payload[name], "number", `payload field ${name} is a number`);
        events.push(payload[name]);
      }
      events.push(Date.now());

      assertOrdered(events);
      resolve();
    });
  });
}


const sandboxManager = new SandboxManager();
sandboxManager.addHold("test running");


// Several of the behaviors of heartbeat prompt are mutually exclusive, so checks are broken up
// into three batches.

/* Batch #1 - General UI, Stars, and telemetry data */
add_task(async function() {
  const targetWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const notificationBox = targetWindow.document.querySelector("#high-priority-global-notificationbox");

  const preCount = notificationBox.childElementCount;
  const hb = new Heartbeat(targetWindow, sandboxManager, {
    testing: true,
    flowId: "test",
    message: "test",
    engagementButtonLabel: undefined,
    learnMoreMessage: "Learn More",
    learnMoreUrl: "https://example.org/learnmore",
  });

  // Check UI
  const learnMoreEl = hb.notice.querySelector(".text-link");
  const messageEl = targetWindow.document.getAnonymousElementByAttribute(hb.notice, "anonid", "messageText");
  Assert.equal(notificationBox.childElementCount, preCount + 1, "Correct number of notifications open");
  Assert.equal(hb.notice.querySelectorAll(".star-x").length, 5, "Correct number of stars");
  Assert.equal(hb.notice.querySelectorAll(".notification-button").length, 0, "Engagement button not shown");
  Assert.equal(learnMoreEl.href, "https://example.org/learnmore", "Learn more url correct");
  Assert.equal(learnMoreEl.value, "Learn More", "Learn more label correct");
  Assert.equal(messageEl.textContent, "test", "Message is correct");

  // Check that when clicking the learn more link, a tab opens with the right URL
  const tabOpenPromise = BrowserTestUtils.waitForNewTab(targetWindow.gBrowser);
  learnMoreEl.click();
  const tab = await tabOpenPromise;
  const tabUrl = await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser, true, url => url && url !== "about:blank");

  Assert.equal(tabUrl, "https://example.org/learnmore", "Learn more link opened the right url");

  const telemetrySentPromise = assertTelemetrySent(hb, ["offeredTS", "learnMoreTS", "closedTS"]);
  // Close notification to trigger telemetry to be sent
  await closeAllNotifications(targetWindow, notificationBox);
  await telemetrySentPromise;
  await BrowserTestUtils.removeTab(tab);
});


// Batch #2 - Engagement buttons
add_task(async function() {
  const targetWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const notificationBox = targetWindow.document.querySelector("#high-priority-global-notificationbox");
  const hb = new Heartbeat(targetWindow, sandboxManager, {
    testing: true,
    flowId: "test",
    message: "test",
    engagementButtonLabel: "Click me!",
    postAnswerUrl: "https://example.org/postAnswer",
    learnMoreMessage: "Learn More",
    learnMoreUrl: "https://example.org/learnMore",
  });
  const engagementButton = hb.notice.querySelector(".notification-button");

  Assert.equal(hb.notice.querySelectorAll(".star-x").length, 0, "Stars not shown");
  Assert.ok(engagementButton, "Engagement button added");
  Assert.equal(engagementButton.label, "Click me!", "Engagement button has correct label");

  const engagementEl = hb.notice.querySelector(".notification-button");
  const tabOpenPromise = BrowserTestUtils.waitForNewTab(targetWindow.gBrowser);
  engagementEl.click();
  const tab = await tabOpenPromise;
  const tabUrl = await BrowserTestUtils.browserLoaded(
        tab.linkedBrowser, true, url => url && url !== "about:blank");
  // the postAnswer url gets query parameters appended onto the end, so use Assert.startsWith instead of Assert.equal
  Assert.ok(tabUrl.startsWith("https://example.org/postAnswer"), "Engagement button opened the right url");

  const telemetrySentPromise = assertTelemetrySent(hb, ["offeredTS", "engagedTS", "closedTS"]);
  // Close notification to trigger telemetry to be sent
  await closeAllNotifications(targetWindow, notificationBox);
  await telemetrySentPromise;
  await BrowserTestUtils.removeTab(tab);
});

// Batch 3 - Closing the window while heartbeat is open
add_task(async function() {
  const targetWindow = await BrowserTestUtils.openNewBrowserWindow();

  const hb = new Heartbeat(targetWindow, sandboxManager, {
    testing: true,
    flowId: "test",
    message: "test",
  });

  const telemetrySentPromise = assertTelemetrySent(hb, ["offeredTS", "windowClosedTS"]);
  // triggers sending ping to normandy
  await BrowserTestUtils.closeWindow(targetWindow);
  await telemetrySentPromise;
});


// Cleanup
add_task(async function() {
  // Make sure the sandbox is clean.
  sandboxManager.removeHold("test running");
  await sandboxManager.isNuked()
    .then(() => ok(true, "sandbox is nuked"))
    .catch(e => ok(false, "sandbox is nuked", e));
});
