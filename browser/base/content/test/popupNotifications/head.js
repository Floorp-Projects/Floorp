var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

/**
 * Called after opening a new window or switching windows, this will wait until
 * we are sure that an attempt to display a notification will not fail.
 */
async function waitForWindowReadyForPopupNotifications(win) {
  // These are the same checks that PopupNotifications.jsm makes before it
  // allows a notification to open.
  await TestUtils.waitForCondition(
    () => win.gBrowser.selectedBrowser.docShellIsActive,
    "The browser should be active"
  );
  await TestUtils.waitForCondition(
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
    BrowserTestUtils.loadURI(browser, url);
  }

  return BrowserTestUtils.browserLoaded(browser, false, url);
}

const PREF_SECURITY_DELAY_INITIAL = Services.prefs.getIntPref(
  "security.notification_enable_delay"
);

// Tests that call setup() should have a `tests` array defined for the actual
// tests to be run.
/* global tests */
function setup() {
  BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/").then(
    goNext
  );
  registerCleanupFunction(() => {
    gBrowser.removeTab(gBrowser.selectedTab);
    PopupNotifications.buttonDelay = PREF_SECURITY_DELAY_INITIAL;
  });
}

function goNext() {
  executeSoon(() => executeSoon(runNextTest));
}

async function runNextTest() {
  if (!tests.length) {
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
      (nextTest.onShown(this) || Promise.resolve()).then(undefined, ex =>
        Assert.ok(false, "onShown failed: " + ex)
      );
    });
    onPopupEvent(
      "popuphidden",
      function() {
        info("[" + nextTest.id + "] popup hidden");
        (nextTest.onHidden(this) || Promise.resolve()).then(
          () => goNext(),
          ex => Assert.ok(false, "onHidden failed: " + ex)
        );
      },
      () => shownState
    );
    info(
      "[" +
        nextTest.id +
        "] added listeners; panel is open: " +
        PopupNotifications.isPanelOpen
    );
  }

  info("[" + nextTest.id + "] running test");
  await nextTest.run();
}

function showNotification(notifyObj) {
  info("Showing notification " + notifyObj.id);
  return PopupNotifications.show(
    notifyObj.browser,
    notifyObj.id,
    notifyObj.message,
    notifyObj.anchorID,
    notifyObj.mainAction,
    notifyObj.secondaryActions,
    notifyObj.options
  );
}

function dismissNotification(popup) {
  info("Dismissing notification " + popup.childNodes[0].id);
  executeSoon(() => EventUtils.synthesizeKey("KEY_Escape"));
}

function BasicNotification(testId) {
  this.browser = gBrowser.selectedBrowser;
  this.id = "test-notification-" + testId;
  this.message = testId + ": Will you allow <> to perform this action?";
  this.anchorID = null;
  this.mainAction = {
    label: "Main Action",
    accessKey: "M",
    callback: ({ source }) => {
      this.mainActionClicked = true;
      this.mainActionSource = source;
    },
  };
  this.secondaryActions = [
    {
      label: "Secondary Action",
      accessKey: "S",
      callback: ({ source }) => {
        this.secondaryActionClicked = true;
        this.secondaryActionSource = source;
      },
    },
  ];
  this.options = {
    name: "http://example.com",
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
    },
  };
}

BasicNotification.prototype.addOptions = function(options) {
  for (let [name, value] of Object.entries(options)) {
    this.options[name] = value;
  }
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
  if (!notification) {
    return;
  }

  // PopupNotifications are not expected to show icons
  // unless popupIconURL or popupIconClass is passed in the options object.
  if (notifyObj.options.popupIconURL || notifyObj.options.popupIconClass) {
    let icon = notification.querySelector(".popup-notification-icon");
    if (notifyObj.id == "geolocation") {
      isnot(icon.getBoundingClientRect().width, 0, "icon for geo displayed");
      ok(
        popup.anchorNode.classList.contains("notification-anchor-icon"),
        "notification anchored to icon"
      );
    }
  }

  let description = notifyObj.message.split("<>");
  let text = {};
  text.start = description[0];
  text.end = description[1];
  is(notification.getAttribute("label"), text.start, "message matches");
  is(
    notification.getAttribute("name"),
    notifyObj.options.name,
    "message matches"
  );
  is(notification.getAttribute("endlabel"), text.end, "message matches");

  is(notification.id, notifyObj.id + "-notification", "id matches");
  if (notifyObj.mainAction) {
    is(
      notification.getAttribute("buttonlabel"),
      notifyObj.mainAction.label,
      "main action label matches"
    );
    is(
      notification.getAttribute("buttonaccesskey"),
      notifyObj.mainAction.accessKey,
      "main action accesskey matches"
    );
  }
  if (notifyObj.secondaryActions && notifyObj.secondaryActions.length) {
    let secondaryAction = notifyObj.secondaryActions[0];
    is(
      notification.getAttribute("secondarybuttonlabel"),
      secondaryAction.label,
      "secondary action label matches"
    );
    is(
      notification.getAttribute("secondarybuttonaccesskey"),
      secondaryAction.accessKey,
      "secondary action accesskey matches"
    );
  }
  // Additional secondary actions appear as menu items.
  let actualExtraSecondaryActions = Array.prototype.filter.call(
    notification.menupopup.childNodes,
    child => child.nodeName == "menuitem"
  );
  let extraSecondaryActions = notifyObj.secondaryActions
    ? notifyObj.secondaryActions.slice(1)
    : [];
  is(
    actualExtraSecondaryActions.length,
    extraSecondaryActions.length,
    "number of extra secondary actions matches"
  );
  extraSecondaryActions.forEach(function(a, i) {
    is(
      actualExtraSecondaryActions[i].getAttribute("label"),
      a.label,
      "label for extra secondary action " + i + " matches"
    );
    is(
      actualExtraSecondaryActions[i].getAttribute("accesskey"),
      a.accessKey,
      "accessKey for extra secondary action " + i + " matches"
    );
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
    if (
      event.target != PopupNotifications.panel ||
      (condition && !condition())
    ) {
      return;
    }
    PopupNotifications.panel.removeEventListener(eventName, listener);
    gActiveListeners.delete(listener);
    executeSoon(() => callback.call(PopupNotifications.panel));
  };
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
  ok(!!notifications.length, "at least one notification displayed");
  let notification = notifications[0];
  info("Triggering main command for notification " + notification.id);
  EventUtils.synthesizeMouseAtCenter(notification.button, {});
}

function triggerSecondaryCommand(popup, index) {
  let notifications = popup.childNodes;
  ok(!!notifications.length, "at least one notification displayed");
  let notification = notifications[0];
  info("Triggering secondary command for notification " + notification.id);

  if (index == 0) {
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});
    return;
  }

  // Extra secondary actions appear in a menu.
  notification.secondaryButton.nextElementSibling.nextElementSibling.focus();

  popup.addEventListener(
    "popupshown",
    function() {
      info("Command popup open for notification " + notification.id);
      // Press down until the desired command is selected. Decrease index by one
      // since the secondary action was handled above.
      for (let i = 0; i <= index - 1; i++) {
        EventUtils.synthesizeKey("KEY_ArrowDown");
      }
      // Activate
      EventUtils.synthesizeKey("KEY_Enter");
    },
    { once: true }
  );

  // One down event to open the popup
  info(
    "Open the popup to trigger secondary command for notification " +
      notification.id
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", {
    altKey: !navigator.platform.includes("Mac"),
  });
}
