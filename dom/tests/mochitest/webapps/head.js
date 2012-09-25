/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * DOMApplicationRegistry._isLaunchable() sometimes returns false right after
 * installation on Mac, perhaps because of a race condition between
 * WebappsInstaller and nsIMacWebAppUtils::pathForAppWithIdentifier().
 * That causes methods like mgmt.getAll() to exclude the app from their results,
 * even though the app is registered and installed.
 *
 * To work around this problem, set DOMApplicationRegistry.allAppsLaunchable
 * to true, which makes _isLaunchable() return true for all registered apps.
 */
function makeAllAppsLaunchable() {
  var Webapps = {};
  Components.utils.import("resource://gre/modules/Webapps.jsm", Webapps);
  var originalValue = Webapps.DOMApplicationRegistry.allAppsLaunchable;
  Webapps.DOMApplicationRegistry.allAppsLaunchable = true;

  // Clean up after ourselves once tests are done so the test page is unloaded.
  window.addEventListener("unload", function restoreAllAppsLaunchable(event) {
    if (event.target == window.document) {
      window.removeEventListener("unload", restoreAllAppsLaunchable, false);
      Webapps.DOMApplicationRegistry.allAppsLaunchable = originalValue;
    }
  }, false);
}

function runAll(steps) {
  SimpleTest.waitForExplicitFinish();

  makeAllAppsLaunchable();

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
