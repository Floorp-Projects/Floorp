Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

/**
 * Allows waiting for an observer notification once.
 *
 * @param topic
 *        Notification topic to observe.
 *
 * @return {Promise}
 * @resolves The array [subject, data] from the observed notification.
 * @rejects Never.
 */
function promiseTopicObserved(topic) {
  return new Promise(resolve => {
    info("Waiting for observer topic " + topic);
    Services.obs.addObserver(function PTO_observe(obsSubject, obsTopic, obsData) {
      Services.obs.removeObserver(PTO_observe, obsTopic);
      resolve([obsSubject, obsData]);
    }, topic);
  });
}

/**
 * Called after opening a new window or switching windows, this will wait until
 * we are sure that an attempt to display a notification will not fail.
 */
async function waitForWindowReadyForPopupNotifications(win) {
  // These are the same checks that PopupNotifications.jsm makes before it
  // allows a notification to open.
  await BrowserTestUtils.waitForCondition(
    () => win.gBrowser.selectedBrowser.docShellIsActive,
    "The browser should be active"
  );
  await BrowserTestUtils.waitForCondition(
    () => Services.focus.activeWindow == win,
    "The window should be active"
  );
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  let browser = tab.linkedBrowser;

  if (url) {
    browser.loadURI(url);
  }

  return BrowserTestUtils.browserLoaded(browser, false, url);
}

const PREF_SECURITY_DELAY_INITIAL = Services.prefs.getIntPref("security.notification_enable_delay");

// Tests that call setup() should have a `tests` array defined for the actual
// tests to be run.
/* global tests */
function setup() {
  BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/")
                  .then(goNext);
  registerCleanupFunction(() => {
    gBrowser.removeTab(gBrowser.selectedTab);
    PopupNotifications.buttonDelay = PREF_SECURITY_DELAY_INITIAL;
  });
}

function goNext() {
  executeSoon(() => executeSoon(runNextTest));
}

async function runNextTest() {
  if (tests.length == 0) {
    executeSoon(finish);
    return;
  }

  let nextTest = tests.shift();
  if (nextTest.onShown) {
    let shownState = false;
    onPopupEvent("popupshowing", function() {
      info("[" + nextTest.id + "] popup showing");
    });
    onPopupEvent("popupshown", function() {
      shownState = true;
      info("[" + nextTest.id + "] popup shown");
      (nextTest.onShown(this) || Promise.resolve())
          .then(undefined, ex => Assert.ok(false, "onShown failed: " + ex));
    });
    onPopupEvent("popuphidden", function() {
      info("[" + nextTest.id + "] popup hidden");
      (nextTest.onHidden(this) || Promise.resolve())
          .then(() => goNext(), ex => Assert.ok(false, "onHidden failed: " + ex));
    }, () => shownState);
    info("[" + nextTest.id + "] added listeners; panel is open: " + PopupNotifications.isPanelOpen);
  }

  info("[" + nextTest.id + "] running test");
  await nextTest.run();
}

function showNotification(notifyObj) {
  info("Showing notification " + notifyObj.id);
  return PopupNotifications.show(notifyObj.browser,
                                 notifyObj.id,
                                 notifyObj.message,
                                 notifyObj.anchorID,
                                 notifyObj.mainAction,
                                 notifyObj.secondaryActions,
                                 notifyObj.options);
}

function dismissNotification(popup) {
  info("Dismissing notification " + popup.childNodes[0].id);
  executeSoon(() => EventUtils.synthesizeKey("VK_ESCAPE", {}));
}

function BasicNotification(testId) {
  this.browser = gBrowser.selectedBrowser;
  this.id = "test-notification-" + testId;
  this.message = "This is popup notification for " + testId;
  this.anchorID = null;
  this.mainAction = {
    label: "Main Action",
    accessKey: "M",
    callback: () => this.mainActionClicked = true
  };
  this.secondaryActions = [
    {
      label: "Secondary Action",
      accessKey: "S",
      callback: () => this.secondaryActionClicked = true
    }
  ];
  this.options = {
    eventCallback: eventName => {
      switch (eventName) {
        case "dismissed":
          this.dismissalCallbackTriggered = true;
          break;
        case "showing":
          this.showingCallbackTriggered = true;
          break;
        case "shown":
          this.shownCallbackTriggered = true;
          break;
        case "removed":
          this.removedCallbackTriggered = true;
          break;
        case "swapping":
          this.swappingCallbackTriggered = true;
          break;
      }
    }
  };
}

BasicNotification.prototype.addOptions = function(options) {
  for (let [name, value] of Object.entries(options))
    this.options[name] = value;
};

function ErrorNotification(testId) {
  BasicNotification.call(this, testId);
  this.mainAction.callback = () => {
    this.mainActionClicked = true;
    throw new Error("Oops!");
  };
  this.secondaryActions[0].callback = () => {
    this.secondaryActionClicked = true;
    throw new Error("Oops!");
  };
}

ErrorNotification.prototype = BasicNotification.prototype;

function checkPopup(popup, notifyObj) {
  info("Checking notification " + notifyObj.id);

  ok(notifyObj.showingCallbackTriggered, "showing callback was triggered");
  ok(notifyObj.shownCallbackTriggered, "shown callback was triggered");

  let notifications = popup.childNodes;
  is(notifications.length, 1, "one notification displayed");
  let notification = notifications[0];
  if (!notification)
    return;
  let icon = document.getAnonymousElementByAttribute(notification, "class",
                                                     "popup-notification-icon");
  if (notifyObj.id == "geolocation") {
    isnot(icon.boxObject.width, 0, "icon for geo displayed");
    ok(popup.anchorNode.classList.contains("notification-anchor-icon"),
       "notification anchored to icon");
  }
  is(notification.getAttribute("label"), notifyObj.message, "message matches");
  is(notification.id, notifyObj.id + "-notification", "id matches");
  if (notifyObj.mainAction) {
    is(notification.getAttribute("buttonlabel"), notifyObj.mainAction.label,
       "main action label matches");
    is(notification.getAttribute("buttonaccesskey"),
       notifyObj.mainAction.accessKey, "main action accesskey matches");
    is(notification.getAttribute("buttonhighlight"),
       (!notifyObj.mainAction.disableHighlight).toString(),
       "main action highlight matches");
  }
  if (notifyObj.secondaryActions && notifyObj.secondaryActions.length > 0) {
    let secondaryAction = notifyObj.secondaryActions[0];
    is(notification.getAttribute("secondarybuttonlabel"), secondaryAction.label,
       "secondary action label matches");
    is(notification.getAttribute("secondarybuttonaccesskey"),
       secondaryAction.accessKey, "secondary action accesskey matches");
  }
  // Additional secondary actions appear as menu items.
  let actualExtraSecondaryActions =
    Array.filter(notification.childNodes, child => child.nodeName == "menuitem");
  let extraSecondaryActions = notifyObj.secondaryActions ? notifyObj.secondaryActions.slice(1) : [];
  is(actualExtraSecondaryActions.length, extraSecondaryActions.length,
     "number of extra secondary actions matches");
  extraSecondaryActions.forEach(function(a, i) {
    is(actualExtraSecondaryActions[i].getAttribute("label"), a.label,
       "label for extra secondary action " + i + " matches");
    is(actualExtraSecondaryActions[i].getAttribute("accesskey"), a.accessKey,
       "accessKey for extra secondary action " + i + " matches");
  });
}

XPCOMUtils.defineLazyGetter(this, "gActiveListeners", () => {
  let listeners = new Map();
  registerCleanupFunction(() => {
    for (let [listener, eventName] of listeners) {
      PopupNotifications.panel.removeEventListener(eventName, listener);
    }
  });
  return listeners;
});

function onPopupEvent(eventName, callback, condition) {
  let listener = event => {
    if (event.target != PopupNotifications.panel ||
        (condition && !condition()))
      return;
    PopupNotifications.panel.removeEventListener(eventName, listener);
    gActiveListeners.delete(listener);
    executeSoon(() => callback.call(PopupNotifications.panel));
  }
  gActiveListeners.set(listener, eventName);
  PopupNotifications.panel.addEventListener(eventName, listener);
}

function waitForNotificationPanel() {
  return new Promise(resolve => {
    onPopupEvent("popupshown", function() {
      resolve(this);
    });
  });
}

function waitForNotificationPanelHidden() {
  return new Promise(resolve => {
    onPopupEvent("popuphidden", function() {
      resolve(this);
    });
  });
}

function triggerMainCommand(popup) {
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];
  info("Triggering main command for notification " + notification.id);
  EventUtils.synthesizeMouseAtCenter(notification.button, {});
}

function triggerSecondaryCommand(popup, index) {
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];
  info("Triggering secondary command for notification " + notification.id);

  if (index == 0) {
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});
    return;
  }

  // Extra secondary actions appear in a menu.
  notification.secondaryButton.nextSibling.nextSibling.focus();

  popup.addEventListener("popupshown", function() {
    info("Command popup open for notification " + notification.id);
    // Press down until the desired command is selected. Decrease index by one
    // since the secondary action was handled above.
    for (let i = 0; i <= index - 1; i++) {
      EventUtils.synthesizeKey("VK_DOWN", {});
    }
    // Activate
    EventUtils.synthesizeKey("VK_RETURN", {});
  }, {once: true});

  // One down event to open the popup
  info("Open the popup to trigger secondary command for notification " + notification.id);
  EventUtils.synthesizeKey("VK_DOWN", { altKey: !navigator.platform.includes("Mac") });
}
