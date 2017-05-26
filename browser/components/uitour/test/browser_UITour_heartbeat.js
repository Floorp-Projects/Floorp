/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

function getHeartbeatNotification(aId, aChromeWindow = window) {
  let notificationBox = aChromeWindow.document.getElementById("high-priority-global-notificationbox");
  // UITour.jsm prefixes the notification box ID with "heartbeat-" to prevent collisions.
  return notificationBox.getNotificationWithValue("heartbeat-" + aId);
}

/**
 * Simulate a click on a rating element in the Heartbeat notification.
 *
 * @param aId
 *        The id of the notification box.
 * @param aScore
 *        The score related to the rating element we want to click on.
 */
function simulateVote(aId, aScore) {
  let notification = getHeartbeatNotification(aId);

  let ratingContainer = notification.childNodes[0];
  ok(ratingContainer, "The notification has a valid rating container.");

  let ratingElement = ratingContainer.getElementsByAttribute("data-score", aScore);
  ok(ratingElement[0], "The rating container contains the requested rating element.");

  ratingElement[0].click();
}

/**
 * Simulate a click on the learn-more link.
 *
 * @param aId
 *        The id of the notification box.
 */
function clickLearnMore(aId) {
  let notification = getHeartbeatNotification(aId);

  let learnMoreLabel = notification.childNodes[2];
  ok(learnMoreLabel, "The notification has a valid learn more label.");

  learnMoreLabel.click();
}

/**
 * Remove the notification box.
 *
 * @param aId
 *        The id of the notification box to remove.
 * @param [aChromeWindow=window]
 *        The chrome window the notification box is in.
 */
function cleanUpNotification(aId, aChromeWindow = window) {
  let notification = getHeartbeatNotification(aId, aChromeWindow);
  notification.close();
}

/**
 * Check telemetry payload for proper format and expected content.
 *
 * @param aPayload
 *        The Telemetry payload to verify
 * @param aFlowId
 *        Expected value of the flowId field.
 * @param aExpectedFields
 *        Array of expected fields. No other fields are allowed.
 */
function checkTelemetry(aPayload, aFlowId, aExpectedFields) {
  // Basic payload format
  is(aPayload.version, 1, "Telemetry ping must have heartbeat version=1");
  is(aPayload.flowId, aFlowId, "Flow ID in the Telemetry ping must match");

  // Check for superfluous fields
  let extraKeys = new Set(Object.keys(aPayload));
  extraKeys.delete("version");
  extraKeys.delete("flowId");

  // Check for expected fields
  for (let field of aExpectedFields) {
    ok(field in aPayload, "The payload should have the field '" + field + "'");
    if (field.endsWith("TS")) {
      let ts = aPayload[field];
      ok(Number.isInteger(ts) && ts > 0, "Timestamp '" + field + "' must be a natural number");
    }
    extraKeys.delete(field);
  }

  is(extraKeys.size, 0, "No unexpected fields in the Telemetry payload");
}

/**
 * Waits for an UITour notification dispatched through |UITour.notify|. This should be
 * done with |gContentAPI.observe|. Unfortunately, in e10s, |gContentAPI.observe| doesn't
 * allow for multiple calls to the same callback, allowing to catch just the first
 * notification.
 *
 * @param aEventName
 *        The notification name to wait for.
 * @return {Promise} Resolved with the data that comes with the event.
 */
function promiseWaitHeartbeatNotification(aEventName) {
  return ContentTask.spawn(gTestTab.linkedBrowser, aEventName, (aContentEventName) => {
        return new Promise(resolve => {
          addEventListener("mozUITourNotification", function listener(event) {
            if (event.detail.event !== aContentEventName) {
              return;
            }
            removeEventListener("mozUITourNotification", listener, false);
            resolve(event.detail.params);
          }, false);
        });
      });
}

/**
 * Waits for UITour notifications dispatched through |UITour.notify|. This works like
 * |promiseWaitHeartbeatNotification|, but waits for all the passed notifications to
 * be received before resolving. If it receives an unaccounted notification, it rejects.
 *
 * @param events
 *        An array of expected notification names to wait for.
 * @return {Promise} Resolved with the data that comes with the event. Rejects with the
 *         name of an undesired notification if received.
 */
function promiseWaitExpectedNotifications(events) {
  return ContentTask.spawn(gTestTab.linkedBrowser, events, contentEvents => {
        let stillToReceive = contentEvents;
        return new Promise((res, rej) => {
          addEventListener("mozUITourNotification", function listener(event) {
            if (stillToReceive.includes(event.detail.event)) {
              // Filter out the received event.
              stillToReceive = stillToReceive.filter(x => x !== event.detail.event);
            } else {
              removeEventListener("mozUITourNotification", listener, false);
              rej(event.detail.event);
            }
            // We still need to catch some notifications. Don't do anything.
            if (stillToReceive.length > 0) {
              return;
            }
            // We don't need to listen for other notifications. Resolve the promise.
            removeEventListener("mozUITourNotification", listener, false);
            res();
          }, false);
        });
      });
}

function validateTimestamp(eventName, timestamp) {
  info("'" + eventName + "' notification received (timestamp " + timestamp.toString() + ").");
  ok(Number.isFinite(timestamp), "Timestamp must be a number.");
}

add_task(async function test_setup() {
  await setup_UITourTest();
  requestLongerTimeout(2);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.uitour.surveyDuration");
  });
});

/**
 * Check that the "stars" heartbeat UI correctly shows and closes.
 */
add_UITour_task(async function test_heartbeat_stars_show() {
  let flowId = "ui-ratefirefox-" + Math.random();
  let engagementURL = "http://example.com";

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(
    ["Heartbeat:NotificationOffered", "Heartbeat:NotificationClosed", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, engagementURL);

  // Validate the returned timestamp.
  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Close the heartbeat notification.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");
  cleanUpNotification(flowId);

  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);

  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received");
  checkTelemetry(data, flowId, ["offeredTS", "closedTS"]);

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Check that the heartbeat UI correctly takes optional icon URL.
 */
add_UITour_task(async function test_heartbeat_take_optional_icon_URL() {
  let flowId = "ui-ratefirefox-" + Math.random();
  let engagementURL = "http://example.com";
  let iconURL = "chrome://branding/content/icon48.png";

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(
    ["Heartbeat:NotificationOffered", "Heartbeat:NotificationClosed", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, engagementURL, null, null, {
    iconURL
  });

  // Validate the returned timestamp.
  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Check the icon URL
  let notification = getHeartbeatNotification(flowId);
  is(notification.image, iconURL, "The optional icon URL is not taken correctly");

  // Close the heartbeat notification.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");
  cleanUpNotification(flowId);

  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);

  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received");
  checkTelemetry(data, flowId, ["offeredTS", "closedTS"]);

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Test that the heartbeat UI correctly works with null engagement URL.
 */
add_UITour_task(async function test_heartbeat_null_engagementURL() {
  let flowId = "ui-ratefirefox-" + Math.random();
  let originalTabCount = gBrowser.tabs.length;

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:Voted", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, null);

  // Validate the returned timestamp.
  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Wait an the Voted, Closed and Telemetry Sent events. They are fired together, so
  // wait for them here.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let votedPromise = promiseWaitHeartbeatNotification("Heartbeat:Voted");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
  simulateVote(flowId, 2);
  data = await votedPromise;
  validateTimestamp("Heartbeat:Voted", data.timestamp);

  // Validate the closing timestamp.
  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);
  is(gBrowser.tabs.length, originalTabCount, "No engagement tab should be opened.");

  // Validate the data we send out.
  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "votedTS", "closedTS", "score"]);
  is(data.score, 2, "Checking Telemetry payload.score");

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Test that the heartbeat UI correctly works with an invalid, but non null, engagement URL.
 */
add_UITour_task(async function test_heartbeat_invalid_engagement_URL() {
  let flowId = "ui-ratefirefox-" + Math.random();
  let originalTabCount = gBrowser.tabs.length;
  let invalidEngagementURL = "invalidEngagement";

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:Voted", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, invalidEngagementURL);

  // Validate the returned timestamp.
  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Wait an the Voted, Closed and Telemetry Sent events. They are fired together, so
  // wait for them here.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let votedPromise = promiseWaitHeartbeatNotification("Heartbeat:Voted");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
  simulateVote(flowId, 2);
  data = await votedPromise;
  validateTimestamp("Heartbeat:Voted", data.timestamp);

  // Validate the closing timestamp.
  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);
  is(gBrowser.tabs.length, originalTabCount, "No engagement tab should be opened.");

  // Validate the data we send out.
  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "votedTS", "closedTS", "score"]);
  is(data.score, 2, "Checking Telemetry payload.score");

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Test that the score is correctly reported.
 */
add_UITour_task(async function test_heartbeat_stars_vote() {
  const expectedScore = 4;
  let originalTabCount = gBrowser.tabs.length;
  let flowId = "ui-ratefirefox-" + Math.random();

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:Voted", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, null);

  // Validate the returned timestamp.
  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Wait an the Voted, Closed and Telemetry Sent events. They are fired together, so
  // wait for them here.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let votedPromise = promiseWaitHeartbeatNotification("Heartbeat:Voted");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
  simulateVote(flowId, expectedScore);
  data = await votedPromise;
  validateTimestamp("Heartbeat:Voted", data.timestamp);
  is(data.score, expectedScore, "Should report a score of " + expectedScore);

  // Validate the closing timestamp and vote.
  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);
  is(gBrowser.tabs.length, originalTabCount, "No engagement tab should be opened.");

  // Validate the data we send out.
  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "votedTS", "closedTS", "score"]);
  is(data.score, expectedScore, "Checking Telemetry payload.score");

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Test that the engagement page is correctly opened when voting.
 */
add_UITour_task(async function test_heartbeat_engagement_tab() {
  let engagementURL = "http://example.com";
  let flowId = "ui-ratefirefox-" + Math.random();
  let originalTabCount = gBrowser.tabs.length;
  const expectedTabCount = originalTabCount + 1;

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:Voted", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, engagementURL);

  // Validate the returned timestamp.
  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Wait an the Voted, Closed and Telemetry Sent events. They are fired together, so
  // wait for them here.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let votedPromise = promiseWaitHeartbeatNotification("Heartbeat:Voted");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
  simulateVote(flowId, 1);
  data = await votedPromise;
  validateTimestamp("Heartbeat:Voted", data.timestamp);

  // Validate the closing timestamp, vote and make sure the engagement page was opened.
  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);
  is(gBrowser.tabs.length, expectedTabCount, "Engagement URL should open in a new tab.");
  gBrowser.removeCurrentTab();

  // Validate the data we send out.
  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "votedTS", "closedTS", "score"]);
  is(data.score, 1, "Checking Telemetry payload.score");

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Test that the engagement button opens the engagement URL.
 */
add_UITour_task(async function test_heartbeat_engagement_button() {
  let engagementURL = "http://example.com";
  let flowId = "ui-engagewithfirefox-" + Math.random();
  let originalTabCount = gBrowser.tabs.length;
  const expectedTabCount = originalTabCount + 1;

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:Engaged", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL, null, null, {
    engagementButtonLabel: "Engage Me",
  });

  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Wait an the Engaged, Closed and Telemetry Sent events. They are fired together, so
  // wait for them here.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let engagedPromise = promiseWaitHeartbeatNotification("Heartbeat:Engaged");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  // Simulate user engagement.
  let notification = getHeartbeatNotification(flowId);
  is(notification.querySelectorAll(".star-x").length, 0, "No stars should be present");
  // The UI was just shown. We can simulate a click on the engagement button.
  let engagementButton = notification.querySelector(".notification-button");
  is(engagementButton.label, "Engage Me", "Check engagement button text");
  engagementButton.doCommand();

  data = await engagedPromise;
  validateTimestamp("Heartbeat:Engaged", data.timestamp);

  // Validate the closing timestamp, vote and make sure the engagement page was opened.
  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);
  is(gBrowser.tabs.length, expectedTabCount, "Engagement URL should open in a new tab.");
  gBrowser.removeCurrentTab();

  // Validate the data we send out.
  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "engagedTS", "closedTS"]);

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Test that the learn more link is displayed and that the page is correctly opened when
 * clicking on it.
 */
add_UITour_task(async function test_heartbeat_learnmore() {
  let dummyURL = "http://example.com";
  let flowId = "ui-ratefirefox-" + Math.random();
  let originalTabCount = gBrowser.tabs.length;
  const expectedTabCount = originalTabCount + 1;

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:LearnMore", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, dummyURL,
                            "What is this?", dummyURL);

  let data = await shownPromise;
  validateTimestamp("Heartbeat:Offered", data.timestamp);

  // Wait an the LearnMore, Closed and Telemetry Sent events. They are fired together, so
  // wait for them here.
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let learnMorePromise = promiseWaitHeartbeatNotification("Heartbeat:LearnMore");
  let pingSentPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  // The UI was just shown. Simulate a click on the learn more link.
  clickLearnMore(flowId);

  data = await learnMorePromise;
  validateTimestamp("Heartbeat:LearnMore", data.timestamp);
  cleanUpNotification(flowId);

  // The notification was closed.
  data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);
  is(gBrowser.tabs.length, expectedTabCount, "Learn more URL should open in a new tab.");
  gBrowser.removeCurrentTab();

  // Validate the data we send out.
  data = await pingSentPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "learnMoreTS", "closedTS"]);

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

add_UITour_task(async function test_invalidEngagementButtonLabel() {
  let engagementURL = "http://example.com";
  let flowId = "invalidEngagementButtonLabel-" + Math.random();

  let eventPromise = promisePageEvent();

  gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                            null, null, {
                              engagementButtonLabel: 42,
                            });

  await eventPromise;
  ok(!isTourBrowser(gBrowser.selectedBrowser),
     "Invalid engagementButtonLabel should prevent init");

})

add_UITour_task(async function test_privateWindowsOnly_noneOpen() {
  let engagementURL = "http://example.com";
  let flowId = "privateWindowsOnly_noneOpen-" + Math.random();

  let eventPromise = promisePageEvent();

  gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                            null, null, {
                              engagementButtonLabel: "Yes!",
                              privateWindowsOnly: true,
                            });

  await eventPromise;
  ok(!isTourBrowser(gBrowser.selectedBrowser),
     "If there are no private windows opened, tour init should be prevented");
})

add_UITour_task(async function test_privateWindowsOnly_notMostRecent() {
  let engagementURL = "http://example.com";
  let flowId = "notMostRecent-" + Math.random();

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let mostRecentWin = await BrowserTestUtils.openNewBrowserWindow();

  let eventPromise = promisePageEvent();

  gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                            null, null, {
                              engagementButtonLabel: "Yes!",
                              privateWindowsOnly: true,
                            });

  await eventPromise;
  is(getHeartbeatNotification(flowId, window), null,
     "Heartbeat shouldn't appear in the default window");
  is(!!getHeartbeatNotification(flowId, privateWin), true,
     "Heartbeat should appear in the most recent private window");
  is(getHeartbeatNotification(flowId, mostRecentWin), null,
     "Heartbeat shouldn't appear in the most recent non-private window");

  await BrowserTestUtils.closeWindow(mostRecentWin);
  await BrowserTestUtils.closeWindow(privateWin);
})

add_UITour_task(async function test_privateWindowsOnly() {
  let engagementURL = "http://example.com";
  let learnMoreURL = "http://example.org/learnmore/";
  let flowId = "ui-privateWindowsOnly-" + Math.random();

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await new Promise((resolve) => {
    gContentAPI.observe(function(aEventName, aData) {
      info(aEventName + " notification received: " + JSON.stringify(aData, null, 2));
      ok(false, "No heartbeat notifications should arrive for privateWindowsOnly");
    }, resolve);
  });

  gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                            "Learn More", learnMoreURL, {
                              engagementButtonLabel: "Yes!",
                              privateWindowsOnly: true,
                            });

  await promisePageEvent();

  ok(isTourBrowser(gBrowser.selectedBrowser), "UITour should have been init for the browser");

  let notification = getHeartbeatNotification(flowId, privateWin);

  is(notification.querySelectorAll(".star-x").length, 0, "No stars should be present");

  info("Test the learn more link.");
  let learnMoreLink = notification.querySelector(".text-link");
  is(learnMoreLink.value, "Learn More", "Check learn more label");
  let learnMoreTabPromise = BrowserTestUtils.waitForNewTab(privateWin.gBrowser, null);
  learnMoreLink.click();
  let learnMoreTab = await learnMoreTabPromise;
  is(learnMoreTab.linkedBrowser.currentURI.host, "example.org", "Check learn more site opened");
  ok(PrivateBrowsingUtils.isBrowserPrivate(learnMoreTab.linkedBrowser), "Ensure the learn more tab is private");
  await BrowserTestUtils.removeTab(learnMoreTab);

  info("Test the engagement button's new tab.");
  let engagementButton = notification.querySelector(".notification-button");
  is(engagementButton.label, "Yes!", "Check engagement button text");
  let engagementTabPromise = BrowserTestUtils.waitForNewTab(privateWin.gBrowser, null);
  engagementButton.doCommand();
  let engagementTab = await engagementTabPromise;
  is(engagementTab.linkedBrowser.currentURI.host, "example.com", "Check enagement site opened");
  ok(PrivateBrowsingUtils.isBrowserPrivate(engagementTab.linkedBrowser), "Ensure the engagement tab is private");
  await BrowserTestUtils.removeTab(engagementTab);

  await BrowserTestUtils.closeWindow(privateWin);
})

/**
 * Test that the survey closes itself after a while and submits Telemetry
 */
add_UITour_task(async function test_telemetry_surveyExpired() {
  let flowId = "survey-expired-" + Math.random();
  let engagementURL = "http://example.com";
  let surveyDuration = 1; // 1 second (pref is in seconds)
  Services.prefs.setIntPref("browser.uitour.surveyDuration", surveyDuration);

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(["Heartbeat:NotificationOffered",
    "Heartbeat:NotificationClosed", "Heartbeat:SurveyExpired", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, engagementURL);

  let expiredPromise = promiseWaitHeartbeatNotification("Heartbeat:SurveyExpired");
  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let pingPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");

  await Promise.all([shownPromise, expiredPromise, closedPromise]);
  // Validate the ping data.
  let data = await pingPromise;
  checkTelemetry(data, flowId, ["offeredTS", "expiredTS", "closedTS"]);

  Services.prefs.clearUserPref("browser.uitour.surveyDuration");

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})

/**
 * Check that certain whitelisted experiment parameters get reflected in the
 * Telemetry ping
 */
add_UITour_task(async function test_telemetry_params() {
  let flowId = "telemetry-params-" + Math.random();
  let engagementURL = "http://example.com";
  let extraParams = {
    "surveyId": "foo",
    "surveyVersion": 1.5,
    "testing": true,
    "notWhitelisted": 123,
  };
  let expectedFields = ["surveyId", "surveyVersion", "testing"];

  // We need to call |gContentAPI.observe| at least once to set a valid |notificationListener|
  // in UITour-lib.js, otherwise no message will get propagated.
  gContentAPI.observe(() => {});

  let receivedExpectedPromise = promiseWaitExpectedNotifications(
    ["Heartbeat:NotificationOffered", "Heartbeat:NotificationClosed", "Heartbeat:TelemetrySent"]);

  // Show the Heartbeat notification and wait for it to be displayed.
  let shownPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationOffered");
  gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!",
                            flowId, engagementURL, null, null, extraParams);
  await shownPromise;

  let closedPromise = promiseWaitHeartbeatNotification("Heartbeat:NotificationClosed");
  let pingPromise = promiseWaitHeartbeatNotification("Heartbeat:TelemetrySent");
  cleanUpNotification(flowId);

  // The notification was closed.
  let data = await closedPromise;
  validateTimestamp("Heartbeat:NotificationClosed", data.timestamp);

  // Validate the data we send out.
  data = await pingPromise;
  info("'Heartbeat:TelemetrySent' notification received.");
  checkTelemetry(data, flowId, ["offeredTS", "closedTS"].concat(expectedFields));
  for (let param of expectedFields) {
    is(data[param], extraParams[param],
       "Whitelisted experiment configs should be copied into Telemetry pings");
  }

  // This rejects whenever an unexpected notification is received.
  await receivedExpectedPromise;
})
