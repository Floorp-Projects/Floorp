/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * TO TEST:
 * - test state saved on doorhanger dismissal
 * - links to switch steps
 * - TOS and PP link clicks
 * - identityList is populated correctly
 */

Services.prefs.setBoolPref("toolkit.identity.debug", true);

XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
                                  "resource://gre/modules/identity/Identity.jsm");

const TEST_ORIGIN = "https://example.com";
const TEST_EMAIL = "user@example.com";

let gTestIndex = 0;
let outerWinId = gBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;

function NotificationBase(aNotId) {
  this.id = aNotId;
}
NotificationBase.prototype = {
  message: TEST_ORIGIN,
  mainAction: {
    label: "",
    callback: function() {
      this.mainActionClicked = true;
    }.bind(this),
  },
  secondaryActions: [],
  options: {
    "identity": {
      origin: TEST_ORIGIN,
      rpId: outerWinId,
    },
  },
};

let tests = [
  {
    name: "test_request_required_typed",

    run: function() {
      setupRPFlow();
      this.notifyOptions = {
        rpId: outerWinId,
        origin: TEST_ORIGIN,
      };
      this.notifyObj = new NotificationBase("identity-request");
      Services.obs.notifyObservers({wrappedJSObject: this.notifyOptions},
                                   "identity-request", null);
    },

    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];

      // Check identity popup state
      let state = notification.identity;
      ok(!state.typedEmail, "Nothing should be typed yet");
      ok(!state.selected, "Identity should not be selected yet");
      ok(!state.termsOfService, "No TOS specified");
      ok(!state.privacyPolicy, "No PP specified");
      is(state.step, 0, "Step should be persisted with default value");
      is(state.rpId, outerWinId, "Check rpId");
      is(state.origin, TEST_ORIGIN, "Check origin");

      is(notification.step, 0, "Should be on the new email step");
      is(notification.chooseEmailLink.hidden, true, "Identity list is empty so link to list view should be hidden");
      is(notification.addEmailLink.parentElement.hidden, true, "We are already on the email input step so choose email pane should be hidden");
      is(notification.emailField.value, "", "Email field should default to empty on a new notification");
      let notifDoc = notification.ownerDocument;
      ok(notifDoc.getAnonymousElementByAttribute(notification, "anonid", "tos").hidden,
         "TOS link should be hidden");
      ok(notifDoc.getAnonymousElementByAttribute(notification, "anonid", "privacypolicy").hidden,
         "PP link should be hidden");

      // Try to continue with a missing email address
      triggerMainCommand(popup);
      is(notification.throbber.style.visibility, "hidden", "is throbber visible");
      ok(!notification.button.disabled, "Button should not be disabled");
      is(window.gIdentitySelected, null, "Check no identity selected");

      // Fill in an invalid email address and try again
      notification.emailField.value = "foo";
      triggerMainCommand(popup);
      is(notification.throbber.style.visibility, "hidden", "is throbber visible");
      ok(!notification.button.disabled, "Button should not be disabled");
      is(window.gIdentitySelected, null, "Check no identity selected");

      // Fill in an email address and try again
      notification.emailField.value = TEST_EMAIL;
      triggerMainCommand(popup);
      is(window.gIdentitySelected.rpId, outerWinId, "Check identity selected rpId");
      is(window.gIdentitySelected.identity, TEST_EMAIL, "Check identity selected email");
      is(notification.identity.selected, TEST_EMAIL, "Check persisted email");
      is(notification.throbber.style.visibility, "visible", "is throbber visible");
      ok(notification.button.disabled, "Button should be disabled");
      ok(notification.emailField.disabled, "Email field should be disabled");
      ok(notification.identityList.disabled, "Identity list should be disabled");

      PopupNotifications.getNotification("identity-request").remove();
    },

    onHidden: function(popup) { },
  },
  {
    name: "test_request_optional",

    run: function() {
      this.notifyOptions = {
        rpId: outerWinId,
        origin: TEST_ORIGIN,
        privacyPolicy: TEST_ORIGIN + "/pp.txt",
        termsOfService: TEST_ORIGIN + "/tos.tzt",
      };
      this.notifyObj = new NotificationBase("identity-request");
      Services.obs.notifyObservers({ wrappedJSObject: this.notifyOptions },
                                   "identity-request", null);
    },

    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];

      // Check identity popup state
      let state = notification.identity;
      ok(!state.typedEmail, "Nothing should be typed yet");
      ok(!state.selected, "Identity should not be selected yet");
      is(state.termsOfService, this.notifyOptions.termsOfService, "Check TOS URL");
      is(state.privacyPolicy, this.notifyOptions.privacyPolicy, "Check PP URL");
      is(state.step, 0, "Step should be persisted with default value");
      is(state.rpId, outerWinId, "Check rpId");
      is(state.origin, TEST_ORIGIN, "Check origin");

      is(notification.step, 0, "Should be on the new email step");
      is(notification.chooseEmailLink.hidden, true, "Identity list is empty so link to list view should be hidden");
      is(notification.addEmailLink.parentElement.hidden, true, "We are already on the email input step so choose email pane should be hidden");
      is(notification.emailField.value, "", "Email field should default to empty on a new notification");
      let notifDoc = notification.ownerDocument;
      let tosLink = notifDoc.getAnonymousElementByAttribute(notification, "anonid", "tos");
      ok(!tosLink.hidden, "TOS link should be visible");
      is(tosLink.href, this.notifyOptions.termsOfService, "Check TOS link URL");
      let ppLink = notifDoc.getAnonymousElementByAttribute(notification, "anonid", "privacypolicy");
      ok(!ppLink.hidden, "PP link should be visible");
      is(ppLink.href, this.notifyOptions.privacyPolicy, "Check PP link URL");

      // Try to continue with a missing email address
      triggerMainCommand(popup);
      is(notification.throbber.style.visibility, "hidden", "is throbber visible");
      ok(!notification.button.disabled, "Button should not be disabled");
      is(window.gIdentitySelected, null, "Check no identity selected");

      // Fill in an invalid email address and try again
      notification.emailField.value = "foo";
      triggerMainCommand(popup);
      is(notification.throbber.style.visibility, "hidden", "is throbber visible");
      ok(!notification.button.disabled, "Button should not be disabled");
      is(window.gIdentitySelected, null, "Check no identity selected");

      // Fill in an email address and try again
      notification.emailField.value = TEST_EMAIL;
      triggerMainCommand(popup);
      is(window.gIdentitySelected.rpId, outerWinId, "Check identity selected rpId");
      is(window.gIdentitySelected.identity, TEST_EMAIL, "Check identity selected email");
      is(notification.identity.selected, TEST_EMAIL, "Check persisted email");
      is(notification.throbber.style.visibility, "visible", "is throbber visible");
      ok(notification.button.disabled, "Button should be disabled");
      ok(notification.emailField.disabled, "Email field should be disabled");
      ok(notification.identityList.disabled, "Identity list should be disabled");

      PopupNotifications.getNotification("identity-request").remove();
    },

    onHidden: function(popup) {},
  },
  {
    name: "test_login_state_changed",
    run: function () {
      this.notifyOptions = {
        rpId: outerWinId,
      };
      this.notifyObj = new NotificationBase("identity-logged-in");
      this.notifyObj.message = "Signed in as: user@example.com";
      this.notifyObj.mainAction.label = "Sign Out";
      this.notifyObj.mainAction.accessKey = "O";
      Services.obs.notifyObservers({ wrappedJSObject: this.notifyOptions },
                                   "identity-login-state-changed", TEST_EMAIL);
      executeSoon(function() {
        PopupNotifications.getNotification("identity-logged-in").anchorElement.click();
      });
    },

    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);

      // Fire the notification that the user is no longer logged-in to close the UI.
      Services.obs.notifyObservers({ wrappedJSObject: this.notifyOptions },
                                   "identity-login-state-changed", null);
    },

    onHidden: function(popup) {},
  },
  {
    name: "test_login_state_changed_logout",
    run: function () {
      this.notifyOptions = {
        rpId: outerWinId,
      };
      this.notifyObj = new NotificationBase("identity-logged-in");
      this.notifyObj.message = "Signed in as: user@example.com";
      this.notifyObj.mainAction.label = "Sign Out";
      this.notifyObj.mainAction.accessKey = "O";
      Services.obs.notifyObservers({ wrappedJSObject: this.notifyOptions },
                                   "identity-login-state-changed", TEST_EMAIL);
      executeSoon(function() {
        PopupNotifications.getNotification("identity-logged-in").anchorElement.click();
      });
    },

    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);

      // This time trigger the Sign Out button and make sure the UI goes away.
      triggerMainCommand(popup);
    },

    onHidden: function(popup) {},
  },
];

function test_auth() {
  let notifyOptions = {
    provId: outerWinId,
    origin: TEST_ORIGIN,
  };

  Services.obs.addObserver(function() {
    // prepare to send auth-complete and close the window
    let winCloseObs = new WindowObserver(function(closedWin) {
      info("closed window");
      finish();
    }, "domwindowclosed");
    Services.ww.registerNotification(winCloseObs);
    Services.obs.notifyObservers(null, "identity-auth-complete", IdentityService.IDP.authenticationFlowSet.authId);

  }, "test-identity-auth-window", false);

  let winObs = new WindowObserver(function(authWin) {
    ok(authWin, "Authentication window opened");
    ok(authWin.contentWindow.location);
  });

  Services.ww.registerNotification(winObs);

  Services.obs.notifyObservers({ wrappedJSObject: notifyOptions },
                               "identity-auth", TEST_ORIGIN + "/auth");
}

function test() {
  waitForExplicitFinish();

  let sitw = {};
  try {
    Components.utils.import("resource:///modules/SignInToWebsite.jsm", sitw);
  } catch (ex) {
    ok(true, "Skip the test since SignInToWebsite.jsm isn't packaged outside outside mozilla-central");
    finish();
    return;
  }

  PopupNotifications.transitionsEnabled = false;

  registerCleanupFunction(cleanUp);

  ok(sitw.SignInToWebsiteUX, "SignInToWebsiteUX object exists");
  if (!Services.prefs.getBoolPref("dom.identity.enabled")) {
    // If the pref isn't enabled then init wasn't called so do that for the test.
    sitw.SignInToWebsiteUX.init();
  }

  // Replace implementation of ID Service functions for testing
  window.selectIdentity = sitw.SignInToWebsiteUX.selectIdentity;
  sitw.SignInToWebsiteUX.selectIdentity = function(aRpId, aIdentity) {
    info("Identity selected: " + aIdentity);
    window.gIdentitySelected = {rpId: aRpId, identity: aIdentity};
  };

  window.setAuthenticationFlow = IdentityService.IDP.setAuthenticationFlow;
  IdentityService.IDP.setAuthenticationFlow = function(aAuthId, aProvId) {
    info("setAuthenticationFlow: " + aAuthId + " : " + aProvId);
    this.authenticationFlowSet = { authId: aAuthId, provId: aProvId };
    Services.obs.notifyObservers(null, "test-identity-auth-window", aAuthId);
  };

  runNextTest();
}

// Cleanup between tests
function resetState() {
  delete window.gIdentitySelected;
  delete IdentityService.IDP.authenticationFlowSet;
  IdentityService.reset();
}

// Cleanup after all tests
function cleanUp() {
  info("cleanup");
  resetState();

  PopupNotifications.transitionsEnabled = true;

  for (let topic in gActiveObservers)
    Services.obs.removeObserver(gActiveObservers[topic], topic);
  for (let eventName in gActiveListeners)
    PopupNotifications.panel.removeEventListener(eventName, gActiveListeners[eventName], false);
  delete IdentityService.RP._rpFlows[outerWinId];

  // Put the JSM functions back to how they were
  IdentityService.IDP.setAuthenticationFlow = window.setAuthenticationFlow;
  delete window.setAuthenticationFlow;

  let sitw = {};
  Components.utils.import("resource:///modules/SignInToWebsite.jsm", sitw);
  sitw.SignInToWebsiteUX.selectIdentity = window.selectIdentity;
  delete window.selectIdentity;
  if (!Services.prefs.getBoolPref("dom.identity.enabled")) {
    sitw.SignInToWebsiteUX.uninit();
  }

  Services.prefs.clearUserPref("toolkit.identity.debug");
}

let gActiveListeners = {};
let gActiveObservers = {};
let gShownState = {};

function runNextTest() {
  let nextTest = tests[gTestIndex];

  function goNext() {
    resetState();
    if (++gTestIndex == tests.length)
      executeSoon(test_auth);
    else
      executeSoon(runNextTest);
  }

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
  } else {
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
        // TODO: needed?
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
  };
  PopupNotifications.panel.addEventListener(eventName, gActiveListeners[eventName], false);
}

function checkPopup(popup, notificationObj) {
  info("[Test #" + gTestIndex + "] checking popup");

  let notifications = popup.childNodes;
  is(notifications.length, 1, "only one notification displayed");
  let notification = notifications[0];
  let icon = document.getAnonymousElementByAttribute(notification, "class", "popup-notification-icon");
  is(notification.getAttribute("label"), notificationObj.message, "message matches");
  is(notification.id, notificationObj.id + "-notification", "id matches");
  if (notificationObj.id != "identity-request" && notificationObj.mainAction) {
    is(notification.getAttribute("buttonlabel"), notificationObj.mainAction.label, "main action label matches");
    is(notification.getAttribute("buttonaccesskey"), notificationObj.mainAction.accessKey, "main action accesskey matches");
  }
  let actualSecondaryActions = notification.childNodes;
  let secondaryActions = notificationObj.secondaryActions || [];
  let actualSecondaryActionsCount = actualSecondaryActions.length;
  if (secondaryActions.length) {
    let lastChild = actualSecondaryActions.item(actualSecondaryActions.length - 1);
    is(lastChild.tagName, "menuseparator", "menuseparator exists");
    actualSecondaryActionsCount--;
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

  notification.button.focus();

  popup.addEventListener("popupshown", function () {
    popup.removeEventListener("popupshown", arguments.callee, false);

    // Press down until the desired command is selected
    for (let i = 0; i <= index; i++)
      EventUtils.synthesizeKey("VK_DOWN", {});

    // Activate
    EventUtils.synthesizeKey("VK_RETURN", {});
  }, false);

  // One down event to open the popup
  EventUtils.synthesizeKey("VK_DOWN", { altKey: (navigator.platform.indexOf("Mac") == -1) });
}

function dismissNotification(popup) {
  info("[Test #" + gTestIndex + "] dismissing notification");
  executeSoon(function () {
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  });
}

function partial(fn) {
  let args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return fn.apply(this, args.concat(Array.prototype.slice.call(arguments)));
  };
}

// create a mock "doc" object, which the Identity Service
// uses as a pointer back into the doc object
function mock_doc(aIdentity, aOrigin, aDoFunc) {
  let mockedDoc = {};
  mockedDoc.id = outerWinId;
  mockedDoc.loggedInEmail = aIdentity;
  mockedDoc.origin = aOrigin;
  mockedDoc['do'] = aDoFunc;
  mockedDoc.doReady = partial(aDoFunc, 'ready');
  mockedDoc.doLogin = partial(aDoFunc, 'login');
  mockedDoc.doLogout = partial(aDoFunc, 'logout');
  mockedDoc.doError = partial(aDoFunc, 'error');
  mockedDoc.doCancel = partial(aDoFunc, 'cancel');
  mockedDoc.doCoffee = partial(aDoFunc, 'coffee');

  return mockedDoc;
}

// takes a list of functions and returns a function that
// when called the first time, calls the first func,
// then the next time the second, etc.
function call_sequentially() {
  let numCalls = 0;
  let funcs = arguments;

  return function() {
    if (!funcs[numCalls]) {
      let argString = Array.prototype.slice.call(arguments).join(",");
      ok(false, "Too many calls: " + argString);
      return;
    }
    funcs[numCalls].apply(funcs[numCalls], arguments);
    numCalls += 1;
  };
}

function setupRPFlow(aIdentity) {
  IdentityService.RP.watch(mock_doc(aIdentity, TEST_ORIGIN, call_sequentially(
    function(action, params) {
      is(action, "ready", "1st callback");
      is(params, null);
    },
    function(action, params) {
      is(action, "logout", "2nd callback");
      is(params, null);
    },
    function(action, params) {
      is(action, "ready", "3rd callback");
      is(params, null);
    }
  )));
}

function WindowObserver(aCallback, aObserveTopic = "domwindowopened") {
  this.observe = function(aSubject, aTopic, aData) {
    if (aTopic != aObserveTopic) {
      return;
    }
    info(aObserveTopic);
    Services.ww.unregisterNotification(this);

    SimpleTest.executeSoon(function() {
      let domWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
      aCallback(domWin);
    });
  };
}
