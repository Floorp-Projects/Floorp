/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function runAll(steps) {
  SimpleTest.waitForExplicitFinish();

  /**
   * On Mac, apps aren't considered launchable right after they've been
   * installed because the OS takes some time to detect them (so
   * nsIMacWebAppUtils::pathForAppWithIdentifier() returns null).
   * That causes methods like mgmt.getAll() to exclude the app from their
   * results, even though the app is installed and is in the registry.
   * See the tests under toolkit/webapps for a viable solution.
   *
   * To work around this problem, set allAppsLaunchable to true, which makes
   * all apps considered as launchable.
   */
  SpecialPowers.setAllAppsLaunchable(true);

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

function confirmNextInstall() {
  var Ci = SpecialPowers.Ci;

  var popupPanel = SpecialPowers.wrap(window).top.
                   QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIWebNavigation).
                   QueryInterface(Ci.nsIDocShell).
                   chromeEventHandler.ownerDocument.defaultView.
                   PopupNotifications.panel;

  function onPopupShown() {
    popupPanel.removeEventListener("popupshown", onPopupShown, false);
    SpecialPowers.wrap(this).childNodes[0].button.doCommand();
  }
  popupPanel.addEventListener("popupshown", onPopupShown, false);
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

  showAlertNotification: function() {
  },
};

AlertsService.init();

SimpleTest.registerCleanupFunction(() => {
  AlertsService.restore();
});
