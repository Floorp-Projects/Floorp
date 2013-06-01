/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  registerCleanupFunction(cleanUp);

  runNextTest();
}

function cleanUp() {
  for (var topic in gActiveObservers)
    Services.obs.removeObserver(gActiveObservers[topic], topic);
  for (var eventName in gActiveListeners)
    PopupNotifications.panel.removeEventListener(eventName, gActiveListeners[eventName], false);
  PopupNotifications.buttonDelay = PREF_SECURITY_DELAY_INITIAL;
}

const PREF_SECURITY_DELAY_INITIAL = Services.prefs.getIntPref("security.notification_enable_delay");

var gActiveListeners = {};
var gActiveObservers = {};
var gShownState = {};

function goNext() {
  if (++gTestIndex == tests.length)
    executeSoon(finish);
  else
    executeSoon(runNextTest);
}

function runNextTest() {
  let nextTest = tests[gTestIndex];

  function addObserver(topic) {
    function observer() {
      Services.obs.removeObserver(observer, "PopupNotifications-" + topic);
      delete gActiveObservers["PopupNotifications-" + topic];

      info("[Test #" + gTestIndex + "] observer for " + topic + " called");
      nextTest[topic]();
      goNext();
    }
    Services.obs.addObserver(observer, "PopupNotifications-" + topic, false);
    gActiveObservers["PopupNotifications-" + topic] = observer;
  }

  if (nextTest.backgroundShow) {
    addObserver("backgroundShow");
  } else if (nextTest.updateNotShowing) {
    addObserver("updateNotShowing");
  } else if (nextTest.onShown) {
    doOnPopupEvent("popupshowing", function () {
      info("[Test #" + gTestIndex + "] popup showing");
    });
    doOnPopupEvent("popupshown", function () {
      gShownState[gTestIndex] = true;
      info("[Test #" + gTestIndex + "] popup shown");
      nextTest.onShown(this);
    });

    // We allow multiple onHidden functions to be defined in an array.  They're
    // called in the order they appear.
    let onHiddenArray = nextTest.onHidden instanceof Array ?
                        nextTest.onHidden :
                        [nextTest.onHidden];
    doOnPopupEvent("popuphidden", function () {
      if (!gShownState[gTestIndex]) {
        // This is expected to happen for test 9, so let's not treat it as a failure.
        info("Popup from test " + gTestIndex + " was hidden before its popupshown fired");
      }

      let onHidden = onHiddenArray.shift();
      info("[Test #" + gTestIndex + "] popup hidden (" + onHiddenArray.length + " hides remaining)");
      executeSoon(function () {
        onHidden.call(nextTest, this);
        if (!onHiddenArray.length)
          goNext();
      }.bind(this));
    }, onHiddenArray.length);
    info("[Test #" + gTestIndex + "] added listeners; panel state: " + PopupNotifications.isPanelOpen);
  }

  info("[Test #" + gTestIndex + "] running test");
  nextTest.run();
}

function doOnPopupEvent(eventName, callback, numExpected) {
  gActiveListeners[eventName] = function (event) {
    if (event.target != PopupNotifications.panel)
      return;
    if (typeof(numExpected) === "number")
      numExpected--;
    if (!numExpected) {
      PopupNotifications.panel.removeEventListener(eventName, gActiveListeners[eventName], false);
      delete gActiveListeners[eventName];
    }

    callback.call(PopupNotifications.panel);
  }
  PopupNotifications.panel.addEventListener(eventName, gActiveListeners[eventName], false);
}

var gTestIndex = 0;
var gNewTab;

function basicNotification() {
  var self = this;
  this.browser = gBrowser.selectedBrowser;
  this.id = "test-notification-" + gTestIndex;
  this.message = "This is popup notification " + this.id + " from test " + gTestIndex;
  this.anchorID = null;
  this.mainAction = {
    label: "Main Action",
    accessKey: "M",
    callback: function () {
      self.mainActionClicked = true;
    }
  };
  this.secondaryActions = [
    {
      label: "Secondary Action",
      accessKey: "S",
      callback: function () {
        self.secondaryActionClicked = true;
      }
    }
  ];
  this.options = {
    eventCallback: function (eventName) {
      switch (eventName) {
        case "dismissed":
          self.dismissalCallbackTriggered = true;
          break;
        case "showing":
          self.showingCallbackTriggered = true;
          break;
        case "shown":
          self.shownCallbackTriggered = true;
          break;
        case "removed":
          self.removedCallbackTriggered = true;
          break;
      }
    }
  };
  this.addOptions = function(options) {
    for (let [name, value] in Iterator(options))
      self.options[name] = value;
  }
}

function errorNotification() {
  var self = this;
  this.browser = gBrowser.selectedBrowser;
  this.id = "test-notification-" + gTestIndex;
  this.message = "This is popup notification " + this.id + " from test " + gTestIndex;
  this.anchorID = null;
  this.mainAction = {
    label: "Main Action",
    accessKey: "M",
    callback: function () {
      self.mainActionClicked = true;
      throw new Error("Oops!");
    }
  };
  this.secondaryActions = [
    {
      label: "Secondary Action",
      accessKey: "S",
      callback: function () {
        self.secondaryActionClicked = true;
        throw new Error("Oops!");
      }
    }
  ];
  this.options = {
    eventCallback: function (eventName) {
      switch (eventName) {
        case "dismissed":
          self.dismissalCallbackTriggered = true;
          break;
        case "showing":
          self.showingCallbackTriggered = true;
          break;
        case "shown":
          self.shownCallbackTriggered = true;
          break;
        case "removed":
          self.removedCallbackTriggered = true;
          break;
      }
    }
  };
  this.addOptions = function(options) {
    for (let [name, value] in Iterator(options))
      self.options[name] = value;
  }
}

var wrongBrowserNotificationObject = new basicNotification();
var wrongBrowserNotification;

var tests = [
  { // Test #0
    run: function () {
      this.notifyObj = new basicNotification();
      showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden: function (popup) {
      ok(this.notifyObj.mainActionClicked, "mainAction was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  { // Test #1
    run: function () {
      this.notifyObj = new basicNotification();
      showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden: function (popup) {
      ok(this.notifyObj.secondaryActionClicked, "secondaryAction was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  { // Test #2
    run: function () {
      this.notifyObj = new basicNotification();
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // test opening a notification for a background browser
  { // Test #3
    run: function () {
      gNewTab = gBrowser.addTab("about:blank");
      isnot(gBrowser.selectedTab, gNewTab, "new tab isn't selected");
      wrongBrowserNotificationObject.browser = gBrowser.getBrowserForTab(gNewTab);
      wrongBrowserNotification = showNotification(wrongBrowserNotificationObject);
    },
    backgroundShow: function () {
      is(PopupNotifications.isPanelOpen, false, "panel isn't open");
      ok(!wrongBrowserNotificationObject.mainActionClicked, "main action wasn't clicked");
      ok(!wrongBrowserNotificationObject.secondaryActionClicked, "secondary action wasn't clicked");
      ok(!wrongBrowserNotificationObject.dismissalCallbackTriggered, "dismissal callback wasn't called");
    }
  },
  // now select that browser and test to see that the notification appeared
  { // Test #4
    run: function () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gNewTab;
    },
    onShown: function (popup) {
      checkPopup(popup, wrongBrowserNotificationObject);
      is(PopupNotifications.isPanelOpen, true, "isPanelOpen getter doesn't lie");

      // switch back to the old browser
      gBrowser.selectedTab = this.oldSelectedTab;
    },
    onHidden: function (popup) {
      // actually remove the notification to prevent it from reappearing
      ok(wrongBrowserNotificationObject.dismissalCallbackTriggered, "dismissal callback triggered due to tab switch");
      wrongBrowserNotification.remove();
      ok(wrongBrowserNotificationObject.removedCallbackTriggered, "removed callback triggered");
      wrongBrowserNotification = null;
    }
  },
  // test that the removed notification isn't shown on browser re-select
  { // Test #5
    run: function () {
      gBrowser.selectedTab = gNewTab;
    },
    updateNotShowing: function () {
      is(PopupNotifications.isPanelOpen, false, "panel isn't open");
      gBrowser.removeTab(gNewTab);
    }
  },
  // Test that two notifications with the same ID result in a single displayed
  // notification.
  { // Test #6
    run: function () {
      this.notifyObj = new basicNotification();
      // Show the same notification twice
      this.notification1 = showNotification(this.notifyObj);
      this.notification2 = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      this.notification2.remove();
    },
    onHidden: function (popup) {
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test that two notifications with different IDs are displayed
  { // Test #7
    run: function () {
      this.testNotif1 = new basicNotification();
      this.testNotif1.message += " 1";
      showNotification(this.testNotif1);
      this.testNotif2 = new basicNotification();
      this.testNotif2.message += " 2";
      this.testNotif2.id += "-2";
      showNotification(this.testNotif2);
    },
    onShown: function (popup) {
      is(popup.childNodes.length, 2, "two notifications are shown");
      // Trigger the main command for the first notification, and the secondary
      // for the second. Need to do mainCommand first since the secondaryCommand
      // triggering is async.
      triggerMainCommand(popup);
      is(popup.childNodes.length, 1, "only one notification left");
      triggerSecondaryCommand(popup, 0);
    },
    onHidden: function (popup) {
      ok(this.testNotif1.mainActionClicked, "main action #1 was clicked");
      ok(!this.testNotif1.secondaryActionClicked, "secondary action #1 wasn't clicked");
      ok(!this.testNotif1.dismissalCallbackTriggered, "dismissal callback #1 wasn't called");

      ok(!this.testNotif2.mainActionClicked, "main action #2 wasn't clicked");
      ok(this.testNotif2.secondaryActionClicked, "secondary action #2 was clicked");
      ok(!this.testNotif2.dismissalCallbackTriggered, "dismissal callback #2 wasn't called");
    }
  },
  // Test notification without mainAction
  { // Test #8
    run: function () {
      this.notifyObj = new basicNotification();
      this.notifyObj.mainAction = null;
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      this.notification.remove();
    }
  },
  // Test two notifications with different anchors
  { // Test #9
    run: function () {
      this.notifyObj = new basicNotification();
      this.firstNotification = showNotification(this.notifyObj);
      this.notifyObj2 = new basicNotification();
      this.notifyObj2.id += "-2";
      this.notifyObj2.anchorID = "addons-notification-icon";
      // Second showNotification() overrides the first
      this.secondNotification = showNotification(this.notifyObj2);
    },
    onShown: function (popup) {
      // This also checks that only one element is shown.
      checkPopup(popup, this.notifyObj2);
      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");
      dismissNotification(popup);
    },
    onHidden: [
      // The second showing triggers a popuphidden event that we should ignore.
      function (popup) {},
      function (popup) {
        // Remove the notifications
        this.firstNotification.remove();
        this.secondNotification.remove();
        ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
        ok(this.notifyObj2.removedCallbackTriggered, "removed callback triggered");
      }
    ]
  },
  // Test optional params
  { // Test #10
    run: function () {
      this.notifyObj = new basicNotification();
      this.notifyObj.secondaryActions = undefined;
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test that icons appear
  { // Test #11
    run: function () {
      this.notifyObj = new basicNotification();
      this.notifyObj.id = "geolocation";
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      let icon = document.getElementById("geo-notification-icon");
      isnot(icon.boxObject.width, 0,
            "geo anchor should be visible after dismissal");
      this.notification.remove();
      is(icon.boxObject.width, 0,
         "geo anchor should not be visible after removal");
    }
  },
  // Test that persistence allows the notification to persist across reloads
  { // Test #12
    run: function () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");

      let self = this;
      loadURI("http://example.com/", function() {
        self.notifyObj = new basicNotification();
        self.notifyObj.addOptions({
          persistence: 2
        });
        self.notification = showNotification(self.notifyObj);
      });
    },
    onShown: function (popup) {
      this.complete = false;

      let self = this;
      loadURI("http://example.org/", function() {
        loadURI("http://example.com/", function() {

          // Next load will remove the notification
          self.complete = true;

          loadURI("http://example.org/");
        });
      });
    },
    onHidden: function (popup) {
      ok(this.complete, "Should only have hidden the notification after 3 page loads");
      ok(this.notifyObj.removedCallbackTriggered, "removal callback triggered");
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that a timeout allows the notification to persist across reloads
  { // Test #13
    run: function () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");

      let self = this;
      loadURI("http://example.com/", function() {
        self.notifyObj = new basicNotification();
        // Set a timeout of 10 minutes that should never be hit
        self.notifyObj.addOptions({
          timeout: Date.now() + 600000
        });
        self.notification = showNotification(self.notifyObj);
      });
    },
    onShown: function (popup) {
      this.complete = false;

      let self = this;
      loadURI("http://example.org/", function() {
        loadURI("http://example.com/", function() {

          // Next load will hide the notification
          self.notification.options.timeout = Date.now() - 1;
          self.complete = true;

          loadURI("http://example.org/");
        });
      });
    },
    onHidden: function (popup) {
      ok(this.complete, "Should only have hidden the notification after the timeout was passed");
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that setting persistWhileVisible allows a visible notification to
  // persist across location changes
  { // Test #14
    run: function () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");

      let self = this;
      loadURI("http://example.com/", function() {
        self.notifyObj = new basicNotification();
        self.notifyObj.addOptions({
          persistWhileVisible: true
        });
        self.notification = showNotification(self.notifyObj);
      });
    },
    onShown: function (popup) {
      this.complete = false;

      let self = this;
      loadURI("http://example.org/", function() {
        loadURI("http://example.com/", function() {

          // Notification should persist across location changes
          self.complete = true;
          dismissNotification(popup);
        });
      });
    },
    onHidden: function (popup) {
      ok(this.complete, "Should only have hidden the notification after it was dismissed");
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that nested icon nodes correctly activate popups
  { // Test #15
    run: function() {
      // Add a temporary box as the anchor with a button
      this.box = document.createElement("box");
      PopupNotifications.iconBox.appendChild(this.box);

      let button = document.createElement("button");
      button.setAttribute("label", "Please click me!");
      this.box.appendChild(button);

      // The notification should open up on the box
      this.notifyObj = new basicNotification();
      this.notifyObj.anchorID = this.box.id = "nested-box";
      this.notifyObj.addOptions({dismissed: true});
      this.notification = showNotification(this.notifyObj);

      EventUtils.synthesizeMouse(button, 1, 1, {});
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      this.notification.remove();
      this.box.parentNode.removeChild(this.box);
    }
  },
  // Test that popupnotifications without popups have anchor icons shown
  { // Test #16
    run: function() {
      let notifyObj = new basicNotification();
      notifyObj.anchorID = "geo-notification-icon";
      notifyObj.addOptions({neverShow: true});
      showNotification(notifyObj);
    },
    updateNotShowing: function() {
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");
    }
  },
  // Test notification "Not Now" menu item
  { // Test #17
    run: function () {
      this.notifyObj = new basicNotification();
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 1);
    },
    onHidden: function (popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test notification close button
  { // Test #18
    run: function () {
      this.notifyObj = new basicNotification();
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      EventUtils.synthesizeMouseAtCenter(notification.closebutton, {});
    },
    onHidden: function (popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test notification when chrome is hidden
  { // Test #19
    run: function () {
      window.locationbar.visible = false;
      this.notifyObj = new basicNotification();
      this.notification = showNotification(this.notifyObj);
      window.locationbar.visible = true;
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      is(popup.anchorNode.className, "tabbrowser-tab", "notification anchored to tab");
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test notification is removed when dismissed if removeOnDismissal is true
  { // Test #20
    run: function () {
      this.notifyObj = new basicNotification();
      this.notifyObj.addOptions({
        removeOnDismissal: true
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test multiple notification icons are shown
  { // Test #21
    run: function () {
      this.notifyObj1 = new basicNotification();
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notification1 = showNotification(this.notifyObj1);

      this.notifyObj2 = new basicNotification();
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notification2 = showNotification(this.notifyObj2);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj2);

      // check notifyObj1 anchor icon is showing
      isnot(document.getElementById("default-notification-icon").boxObject.width, 0,
            "default anchor should be visible");
      // check notifyObj2 anchor icon is showing
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");

      dismissNotification(popup);
    },
    onHidden: [
      function (popup) {
      },
      function (popup) {
        this.notification1.remove();
        ok(this.notifyObj1.removedCallbackTriggered, "removed callback triggered");

        this.notification2.remove();
        ok(this.notifyObj2.removedCallbackTriggered, "removed callback triggered");
      }
    ]
  },
  // Test that multiple notification icons are removed when switching tabs
  { // Test #22
    run: function () {
      // show the notification on old tab.
      this.notifyObjOld = new basicNotification();
      this.notifyObjOld.anchorID = "default-notification-icon";
      this.notificationOld = showNotification(this.notifyObjOld);

      // switch tab
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");

      // show the notification on new tab.
      this.notifyObjNew = new basicNotification();
      this.notifyObjNew.anchorID = "geo-notification-icon";
      this.notificationNew = showNotification(this.notifyObjNew);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObjNew);

      // check notifyObjOld anchor icon is removed
      is(document.getElementById("default-notification-icon").boxObject.width, 0,
         "default anchor shouldn't be visible");
      // check notifyObjNew anchor icon is showing
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");

      dismissNotification(popup);
    },
    onHidden: [
      function (popup) {
      },
      function (popup) {
        this.notificationNew.remove();
        gBrowser.removeTab(gBrowser.selectedTab);

        gBrowser.selectedTab = this.oldSelectedTab;
        this.notificationOld.remove();
      }
    ]
  },
  { // Test #23 - test security delay - too early
    run: function () {
      // Set the security delay to 100s
      PopupNotifications.buttonDelay = 100000;

      this.notifyObj = new basicNotification();
      showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);

      // Wait to see if the main command worked
      executeSoon(function delayedDismissal() {
        dismissNotification(popup);
      });

    },
    onHidden: function (popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked because it was too soon");
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback was triggered");
    }
  },
  { // Test #24  - test security delay - after delay
    run: function () {
      // Set the security delay to 10ms
      PopupNotifications.buttonDelay = 10;

      this.notifyObj = new basicNotification();
      showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);

      // Wait until after the delay to trigger the main action
      setTimeout(function delayedDismissal() {
        triggerMainCommand(popup);
      }, 500);

    },
    onHidden: function (popup) {
      ok(this.notifyObj.mainActionClicked, "mainAction was clicked after the delay");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback was not triggered");
      PopupNotifications.buttonDelay = PREF_SECURITY_DELAY_INITIAL;
    }
  },
  { // Test #25 - reload removes notification
    run: function () {
      loadURI("http://example.com/", function() {
        let notifyObj = new basicNotification();
        notifyObj.options.eventCallback = function (eventName) {
          if (eventName == "removed") {
            ok(true, "Notification removed in background tab after reloading");
            executeSoon(function () {
              goNext();
            });
          }
        };
        showNotification(notifyObj);
        executeSoon(function () {
          gBrowser.selectedBrowser.reload();
        });
      });
    }
  },
  { // Test #26 - location change in background tab removes notification
    run: function () {
      let oldSelectedTab = gBrowser.selectedTab;
      let newTab = gBrowser.addTab("about:blank");
      gBrowser.selectedTab = newTab;

      loadURI("http://example.com/", function() {
        gBrowser.selectedTab = oldSelectedTab;
        let browser = gBrowser.getBrowserForTab(newTab);

        let notifyObj = new basicNotification();
        notifyObj.browser = browser;
        notifyObj.options.eventCallback = function (eventName) {
          if (eventName == "removed") {
            ok(true, "Notification removed in background tab after reloading");
            executeSoon(function () {
              gBrowser.removeTab(newTab);
              goNext();
            });
          }
        };
        showNotification(notifyObj);
        executeSoon(function () {
          browser.reload();
        });
      });
    }
  },
  { // Test #27 -  Popup notification anchor shouldn't disappear when a notification with the same ID is re-added in a background tab
    run: function () {
      loadURI("http://example.com/", function () {
        let originalTab = gBrowser.selectedTab;
        let bgTab = gBrowser.addTab("about:blank");
        gBrowser.selectedTab = bgTab;
        loadURI("http://example.com/", function () {
          let anchor = document.createElement("box");
          anchor.id = "test26-anchor";
          anchor.className = "notification-anchor-icon";
          PopupNotifications.iconBox.appendChild(anchor);

          gBrowser.selectedTab = originalTab;

          let fgNotifyObj = new basicNotification();
          fgNotifyObj.anchorID = anchor.id;
          fgNotifyObj.options.dismissed = true;
          let fgNotification = showNotification(fgNotifyObj);

          let bgNotifyObj = new basicNotification();
          bgNotifyObj.anchorID = anchor.id;
          bgNotifyObj.browser = gBrowser.getBrowserForTab(bgTab);
          // show the notification in the background tab ...
          let bgNotification = showNotification(bgNotifyObj);
          // ... and re-show it
          bgNotification = showNotification(bgNotifyObj);

          ok(fgNotification.id, "notification has id");
          is(fgNotification.id, bgNotification.id, "notification ids are the same");
          is(anchor.getAttribute("showing"), "true", "anchor still showing");

          fgNotification.remove();
          gBrowser.removeTab(bgTab);
          goNext();
        });
      });
    }
  },
  { // Test #28 - location change in embedded frame removes notification
    run: function () {
      loadURI("data:text/html,<iframe id='iframe' src='http://example.com/'>", function () {
        let notifyObj = new basicNotification();
        notifyObj.options.eventCallback = function (eventName) {
          if (eventName == "removed") {
            ok(true, "Notification removed in background tab after reloading");
            executeSoon(goNext);
          }
        };
        showNotification(notifyObj);
        executeSoon(function () {
          content.document.getElementById("iframe")
                          .setAttribute("src", "http://example.org/");
        });
      });
    }
  },
  { // Test #29 - Popup Notifications should catch exceptions from callbacks
    run: function () {
      let callbackCount = 0;
      this.testNotif1 = new basicNotification();
      this.testNotif1.message += " 1";
      showNotification(this.testNotif1);
      this.testNotif1.options.eventCallback = function (eventName) {
        info("notifyObj1.options.eventCallback: " + eventName);
        if (eventName == "dismissed") {
          throw new Error("Oops 1!");
          if (++callbackCount == 2) {
            executeSoon(goNext);
          }
        }
      };

      this.testNotif2 = new basicNotification();
      this.testNotif2.message += " 2";
      this.testNotif2.id += "-2";
      this.testNotif2.options.eventCallback = function (eventName) {
        info("notifyObj2.options.eventCallback: " + eventName);
        if (eventName == "dismissed") {
          throw new Error("Oops 2!");
          if (++callbackCount == 2) {
            executeSoon(goNext);
          }
        }
      };
      showNotification(this.testNotif2);
    },
    onShown: function (popup) {
      is(popup.childNodes.length, 2, "two notifications are shown");
      dismissNotification(popup);
    },
    onHidden: function () {}
  },
  { // Test #30 - Popup Notifications main actions should catch exceptions from callbacks
    run: function () {
      this.testNotif = new errorNotification();
      showNotification(this.testNotif);
    },
    onShown: function (popup) {
      checkPopup(popup, this.testNotif);
      triggerMainCommand(popup);
    },
    onHidden: function (popup) {
      ok(this.testNotif.mainActionClicked, "main action has been triggered");
    }
  },
  { // Test #31 - Popup Notifications secondary actions should catch exceptions from callbacks
    run: function () {
      this.testNotif = new errorNotification();
      showNotification(this.testNotif);
    },
    onShown: function (popup) {
      checkPopup(popup, this.testNotif);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden: function (popup) {
      ok(this.testNotif.secondaryActionClicked, "secondary action has been triggered");
    }
  },
  { // Test #32 -  Existing popup notification shouldn't disappear when adding a dismissed notification
    run: function () {
      this.notifyObj1 = new basicNotification();
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notification1 = showNotification(this.notifyObj1);
    },
    onShown: function (popup) {
      // Now show a dismissed notification, and check that it doesn't clobber
      // the showing one.
      this.notifyObj2 = new basicNotification();
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notifyObj2.options.dismissed = true;
      this.notification2 = showNotification(this.notifyObj2);

      checkPopup(popup, this.notifyObj1);

      // check that both anchor icons are showing
      is(document.getElementById("default-notification-icon").getAttribute("showing"), "true",
         "notification1 anchor should be visible");
      is(document.getElementById("geo-notification-icon").getAttribute("showing"), "true",
         "notification2 anchor should be visible");

      dismissNotification(popup);
    },
    onHidden: function(popup) {
      this.notification1.remove();
      this.notification2.remove();
    }
  },
  { // Test #33 - Showing should be able to modify the popup data
    run: function() {
      this.notifyObj = new basicNotification();
      var normalCallback = this.notifyObj.options.eventCallback;
      this.notifyObj.options.eventCallback = function (eventName) {
        if (eventName == "showing") {
          this.mainAction.label = "Alternate Label";
        }
        normalCallback.call(this, eventName);
      };
      showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      // checkPopup checks for the matching label. Note that this assumes that
      // this.notifyObj.mainAction is the same as notification.mainAction,
      // which could be a problem if we ever decided to deep-copy.
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden: function() { }
  }
];

function showNotification(notifyObj) {
  return PopupNotifications.show(notifyObj.browser,
                                 notifyObj.id,
                                 notifyObj.message,
                                 notifyObj.anchorID,
                                 notifyObj.mainAction,
                                 notifyObj.secondaryActions,
                                 notifyObj.options);
}

function checkPopup(popup, notificationObj) {
  info("[Test #" + gTestIndex + "] checking popup");

  ok(notificationObj.showingCallbackTriggered, "showing callback was triggered");
  ok(notificationObj.shownCallbackTriggered, "shown callback was triggered");

  let notifications = popup.childNodes;
  is(notifications.length, 1, "one notification displayed");
  let notification = notifications[0];
  if (!notification)
    return;
  let icon = document.getAnonymousElementByAttribute(notification, "class", "popup-notification-icon");
  if (notificationObj.id == "geolocation") {
    isnot(icon.boxObject.width, 0, "icon for geo displayed");
    is(popup.anchorNode.className, "notification-anchor-icon", "notification anchored to icon");
  }
  is(notification.getAttribute("label"), notificationObj.message, "message matches");
  is(notification.id, notificationObj.id + "-notification", "id matches");
  if (notificationObj.mainAction) {
    is(notification.getAttribute("buttonlabel"), notificationObj.mainAction.label, "main action label matches");
    is(notification.getAttribute("buttonaccesskey"), notificationObj.mainAction.accessKey, "main action accesskey matches");
  }
  let actualSecondaryActions = Array.filter(notification.childNodes,
                                            function (child) child.nodeName == "menuitem");
  let secondaryActions = notificationObj.secondaryActions || [];
  let actualSecondaryActionsCount = actualSecondaryActions.length;
  if (secondaryActions.length) {
    is(notification.lastChild.tagName, "menuseparator", "menuseparator exists");
  }
  is(actualSecondaryActionsCount, secondaryActions.length, actualSecondaryActions.length + " secondary actions");
  secondaryActions.forEach(function (a, i) {
    is(actualSecondaryActions[i].getAttribute("label"), a.label, "label for secondary action " + i + " matches");
    is(actualSecondaryActions[i].getAttribute("accesskey"), a.accessKey, "accessKey for secondary action " + i + " matches");
  });
}

function triggerMainCommand(popup) {
  info("[Test #" + gTestIndex + "] triggering main command");
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];

  // 20, 10 so that the inner button is hit
  EventUtils.synthesizeMouse(notification.button, 20, 10, {});
}

function triggerSecondaryCommand(popup, index) {
  info("[Test #" + gTestIndex + "] triggering secondary command");
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];

  // Cancel the arrow panel slide-in transition (bug 767133) such that
  // it won't interfere with us interacting with the dropdown.
  document.getAnonymousNodes(popup)[0].style.transition = "none";

  notification.button.focus();

  popup.addEventListener("popupshown", function () {
    popup.removeEventListener("popupshown", arguments.callee, false);

    // Press down until the desired command is selected
    for (let i = 0; i <= index; i++)
      EventUtils.synthesizeKey("VK_DOWN", {});

    // Activate
    EventUtils.synthesizeKey("VK_ENTER", {});
  }, false);

  // One down event to open the popup
  EventUtils.synthesizeKey("VK_DOWN", { altKey: !navigator.platform.contains("Mac") });
}

function loadURI(uri, callback) {
  if (callback) {
    gBrowser.addEventListener("load", function() {
      // Ignore the about:blank load
      if (gBrowser.currentURI.spec == "about:blank")
        return;

      gBrowser.removeEventListener("load", arguments.callee, true);

      callback();
    }, true);
  }
  gBrowser.loadURI(uri);
}

function dismissNotification(popup) {
  info("[Test #" + gTestIndex + "] dismissing notification");
  executeSoon(function () {
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  });
}
