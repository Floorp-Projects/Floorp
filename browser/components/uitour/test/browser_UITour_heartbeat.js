/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  UITourTest();
}

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

let tests = [
  /**
   * Check that the "stars" heartbeat UI correctly shows and closes.
   */
  function test_heartbeat_stars_show(done) {
    let flowId = "ui-ratefirefox-" + Math.random();
    let engagementURL = "http://example.com";

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          cleanUpNotification(flowId);
          break;
        }
        case "Heartbeat:NotificationClosed": {
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          done();
          break;
        }
        default:
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
      }
    });

    gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, engagementURL);
  },

  /**
   * Test that the heartbeat UI correctly works with null engagement URL.
   */
  function test_heartbeat_null_engagementURL(done) {
    let flowId = "ui-ratefirefox-" + Math.random();
    let originalTabCount = gBrowser.tabs.length;

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
          simulateVote(flowId, 2);
          break;
        }
        case "Heartbeat:Voted": {
          info("'Heartbeat:Voted' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          break;
        }
        case "Heartbeat:NotificationClosed": {
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          is(gBrowser.tabs.length, originalTabCount, "No engagement tab should be opened.");
          done();
          break;
        }
        default:
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
      }
    });

    gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, null);
  },

   /**
   * Test that the heartbeat UI correctly works with an invalid, but non null, engagement URL.
   */
  function test_heartbeat_invalid_engagement_URL(done) {
    let flowId = "ui-ratefirefox-" + Math.random();
    let originalTabCount = gBrowser.tabs.length;
    let invalidEngagementURL = "invalidEngagement";

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
          simulateVote(flowId, 2);
          break;
        }
        case "Heartbeat:Voted": {
          info("'Heartbeat:Voted' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          break;
        }
        case "Heartbeat:NotificationClosed": {
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          is(gBrowser.tabs.length, originalTabCount, "No engagement tab should be opened.");
          done();
          break;
        }
        default:
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
      }
    });

    gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, invalidEngagementURL);
  },

  /**
   * Test that the score is correctly reported.
   */
  function test_heartbeat_stars_vote(done) {
    const expectedScore = 4;
    let flowId = "ui-ratefirefox-" + Math.random();

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
          simulateVote(flowId, expectedScore);
          break;
        }
        case "Heartbeat:Voted": {
          info("'Heartbeat:Voted' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          is(aData.score, expectedScore, "Should report a score of " + expectedScore);
          break;
        }
        case "Heartbeat:NotificationClosed": {
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          done();
          break;
        }
        default:
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
      }
    });

    gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, null);
  },

  /**
   * Test that the engagement page is correctly opened when voting.
   */
  function test_heartbeat_engagement_tab(done) {
    let engagementURL = "http://example.com";
    let flowId = "ui-ratefirefox-" + Math.random();
    let originalTabCount = gBrowser.tabs.length;
    const expectedTabCount = originalTabCount + 1;
    let heartbeatVoteSeen = false;

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          // The UI was just shown. We can simulate a click on a rating element (i.e., "star").
          simulateVote(flowId, 1);
          break;
        }
        case "Heartbeat:Voted": {
          info("'Heartbeat:Voted' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          heartbeatVoteSeen = true;
          break;
        }
        case "Heartbeat:NotificationClosed": {
          ok(heartbeatVoteSeen, "Heartbeat vote should have been received");
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          is(gBrowser.tabs.length, expectedTabCount, "Engagement URL should open in a new tab.");
          gBrowser.removeCurrentTab();
          done();
          break;
        }
        default:
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
      }
    });

    gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, engagementURL);
  },

  /**
   * Test that the engagement button opens the engagement URL.
   */
  function test_heartbeat_engagement_button(done) {
    let engagementURL = "http://example.com";
    let flowId = "ui-engagewithfirefox-" + Math.random();
    let originalTabCount = gBrowser.tabs.length;
    const expectedTabCount = originalTabCount + 1;
    let heartbeatEngagedSeen = false;

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          let notification = getHeartbeatNotification(flowId);
          is(notification.querySelectorAll(".star-x").length, 0, "No stars should be present");
          // The UI was just shown. We can simulate a click on the engagement button.
          let engagementButton = notification.querySelector(".notification-button");
          is(engagementButton.label, "Engage Me", "Check engagement button text");
          engagementButton.doCommand();
          break;
        }
        case "Heartbeat:Engaged": {
          info("'Heartbeat:Engaged' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          heartbeatEngagedSeen = true;
          break;
        }
        case "Heartbeat:NotificationClosed": {
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(heartbeatEngagedSeen, "Heartbeat:Engaged should have been received");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          is(gBrowser.tabs.length, expectedTabCount, "Engagement URL should open in a new tab.");
          gBrowser.removeCurrentTab();
          executeSoon(done);
          break;
        }
        default: {
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
        }
      }
    });

    gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL, null, null, {
      engagementButtonLabel: "Engage Me",
    });
  },

  /**
   * Test that the learn more link is displayed and that the page is correctly opened when
   * clicking on it.
   */
  function test_heartbeat_learnmore(done) {
    let dummyURL = "http://example.com";
    let flowId = "ui-ratefirefox-" + Math.random();
    let originalTabCount = gBrowser.tabs.length;
    const expectedTabCount = originalTabCount + 1;

    gContentAPI.observe(function (aEventName, aData) {
      switch (aEventName) {
        case "Heartbeat:NotificationOffered": {
          info("'Heartbeat:Offered' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          // The UI was just shown. Simulate a click on the learn more link.
          clickLearnMore(flowId);
          break;
        }
        case "Heartbeat:LearnMore": {
          info("'Heartbeat:LearnMore' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          cleanUpNotification(flowId);
          break;
        }
        case "Heartbeat:NotificationClosed": {
          info("'Heartbeat:NotificationClosed' notification received (timestamp " + aData.timestamp.toString() + ").");
          ok(Number.isFinite(aData.timestamp), "Timestamp must be a number.");
          is(gBrowser.tabs.length, expectedTabCount, "Learn more URL should open in a new tab.");
          gBrowser.removeCurrentTab();
          done();
          break;
        }
        default:
          // We are not expecting other states for this test.
          ok(false, "Unexpected notification received: " + aEventName);
      }
    });

    gContentAPI.showHeartbeat("How would you rate Firefox?", "Thank you!", flowId, dummyURL,
                              "What is this?", dummyURL);
  },

  taskify(function test_invalidEngagementButtonLabel(done) {
    let engagementURL = "http://example.com";
    let flowId = "invalidEngagementButtonLabel-" + Math.random();

    let eventPromise = promisePageEvent();

    gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                              null, null, {
                                engagementButtonLabel: 42,
                              });

    yield eventPromise;
    ok(!isTourBrowser(gBrowser.selectedBrowser),
       "Invalid engagementButtonLabel should prevent init");

  }),

  taskify(function test_privateWindowsOnly_noneOpen(done) {
    let engagementURL = "http://example.com";
    let flowId = "privateWindowsOnly_noneOpen-" + Math.random();

    let eventPromise = promisePageEvent();

    gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                              null, null, {
                                engagementButtonLabel: "Yes!",
                                privateWindowsOnly: true,
                              });

    yield eventPromise;
    ok(!isTourBrowser(gBrowser.selectedBrowser),
       "If there are no private windows opened, tour init should be prevented");
  }),

  taskify(function test_privateWindowsOnly_notMostRecent(done) {
    let engagementURL = "http://example.com";
    let flowId = "notMostRecent-" + Math.random();

    let privateWin = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
    let mostRecentWin = yield BrowserTestUtils.openNewBrowserWindow();

    let eventPromise = promisePageEvent();

    gContentAPI.showHeartbeat("Do you want to engage with us?", "Thank you!", flowId, engagementURL,
                              null, null, {
                                engagementButtonLabel: "Yes!",
                                privateWindowsOnly: true,
                              });

    yield eventPromise;
    is(getHeartbeatNotification(flowId, window), null,
       "Heartbeat shouldn't appear in the default window");
    is(!!getHeartbeatNotification(flowId, privateWin), true,
       "Heartbeat should appear in the most recent private window");
    is(getHeartbeatNotification(flowId, mostRecentWin), null,
       "Heartbeat shouldn't appear in the most recent non-private window");

    yield BrowserTestUtils.closeWindow(mostRecentWin);
    yield BrowserTestUtils.closeWindow(privateWin);
  }),

  taskify(function test_privateWindowsOnly() {
    let engagementURL = "http://example.com";
    let learnMoreURL = "http://example.org/learnmore/";
    let flowId = "ui-privateWindowsOnly-" + Math.random();

    let privateWin = yield BrowserTestUtils.openNewBrowserWindow({ private: true });

    yield new Promise((resolve) => {
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

    yield promisePageEvent();

    ok(isTourBrowser(gBrowser.selectedBrowser), "UITour should have been init for the browser");

    let notification = getHeartbeatNotification(flowId, privateWin);

    is(notification.querySelectorAll(".star-x").length, 0, "No stars should be present");

    info("Test the learn more link.");
    let learnMoreLink = notification.querySelector(".text-link");
    is(learnMoreLink.value, "Learn More", "Check learn more label");
    let learnMoreTabPromise = BrowserTestUtils.waitForNewTab(privateWin.gBrowser, null);
    learnMoreLink.click();
    let learnMoreTab = yield learnMoreTabPromise;
    is(learnMoreTab.linkedBrowser.currentURI.host, "example.org", "Check learn more site opened");
    ok(PrivateBrowsingUtils.isBrowserPrivate(learnMoreTab.linkedBrowser), "Ensure the learn more tab is private");
    yield BrowserTestUtils.removeTab(learnMoreTab);

    info("Test the engagement button's new tab.");
    let engagementButton = notification.querySelector(".notification-button");
    is(engagementButton.label, "Yes!", "Check engagement button text");
    let engagementTabPromise = BrowserTestUtils.waitForNewTab(privateWin.gBrowser, null);
    engagementButton.doCommand();
    let engagementTab = yield engagementTabPromise;
    is(engagementTab.linkedBrowser.currentURI.host, "example.com", "Check enagement site opened");
    ok(PrivateBrowsingUtils.isBrowserPrivate(engagementTab.linkedBrowser), "Ensure the engagement tab is private");
    yield BrowserTestUtils.removeTab(engagementTab);

    yield BrowserTestUtils.closeWindow(privateWin);
  }),
];
