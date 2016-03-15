/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function runAll(steps) {
  SimpleTest.waitForExplicitFinish();

  // Clone the array so we don't modify the original.
  steps = steps.concat();
  function next() {
    if (steps.length) {
      steps.shift()(next);
    }
    else {
      SimpleTest.finish();
    }
  }
  next();
}

function confirmNextPopup() {
  var Ci = SpecialPowers.Ci;

  var popupNotifications = SpecialPowers.wrap(window).top.
                           QueryInterface(Ci.nsIInterfaceRequestor).
                           getInterface(Ci.nsIWebNavigation).
                           QueryInterface(Ci.nsIDocShell).
                           chromeEventHandler.ownerDocument.defaultView.
                           PopupNotifications;

  var popupPanel = popupNotifications.panel;

  function onPopupShown() {
    popupPanel.removeEventListener("popupshown", onPopupShown, false);
    SpecialPowers.wrap(this).childNodes[0].button.doCommand();
    popupNotifications._dismiss();
  }
  popupPanel.addEventListener("popupshown", onPopupShown, false);
}

function promiseNoPopup() {
  var Ci = SpecialPowers.Ci;

  var popupNotifications = SpecialPowers.wrap(window).top.
                           QueryInterface(Ci.nsIInterfaceRequestor).
                           getInterface(Ci.nsIWebNavigation).
                           QueryInterface(Ci.nsIDocShell).
                           chromeEventHandler.ownerDocument.defaultView.
                           PopupNotifications;

  return new Promise((resolve) => {
    var tries = 0;
    var interval = setInterval(function() {
      if (tries >= 30) {
        ok(true, "The webapps-install notification didn't appear");
        moveOn();
      }

      if (popupNotifications.getNotification("webapps-install")) {
        ok(false, "Found the webapps-install notification");
        moveOn();
      }
      tries++;
    }, 100);

    var moveOn = () => {
      clearInterval(interval);
      resolve();
    };
  });
}

// We need to mock the Alerts service, otherwise the alert that is shown
// at the end of an installation makes the test leak the app's icon.

const CID = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID();
const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const ALERTS_SERVICE_CID = Components.ID(Cc[ALERTS_SERVICE_CONTRACT_ID].number);

var AlertsService = {
  classID: Components.ID(CID),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory,
                                         Ci.nsIAlertsService]),

  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    return this.QueryInterface(aIID);
  },

  init: function() {
    Components.manager.nsIComponentRegistrar.registerFactory(this.classID,
      "", ALERTS_SERVICE_CONTRACT_ID, this);
  },

  restore: function() {
    Components.manager.nsIComponentRegistrar.registerFactory(ALERTS_SERVICE_CID,
      "", ALERTS_SERVICE_CONTRACT_ID, null);
  },

  showAlert: function() {
  },

  showAlertNotification: function() {
  },
};

AlertsService.init();

SimpleTest.registerCleanupFunction(() => {
  AlertsService.restore();
});
