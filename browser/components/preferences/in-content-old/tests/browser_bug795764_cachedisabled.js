/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();

  let prefs = [
    "browser.cache.offline.enable",
    "browser.cache.disk.enable",
    "browser.cache.memory.enable",
  ];
  for (let pref of prefs) {
    Services.prefs.setBoolPref(pref, false);
  }

  // Adding one fake site so that the SiteDataManager would run.
  // Otherwise, without any site then it would just return so we would end up in not testing SiteDataManager.
  let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("https://www.foo.com");
  Services.perms.addFromPrincipal(principal, "persistent-storage", Ci.nsIPermissionManager.ALLOW_ACTION);

  registerCleanupFunction(function() {
    for (let pref of prefs) {
      Services.prefs.clearUserPref(pref);
    }
    Services.perms.removeFromPrincipal(principal, "persistent-storage");
  });

  open_preferences(runTest);
}

function runTest(win) {
  is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");

  let tab = win.document;
  let elements = tab.getElementById("mainPrefPane").children;
  let offlineGroupDisabled = !SpecialPowers.getBoolPref("browser.preferences.offlineGroup.enabled");

  // Test if advanced pane is opened correctly
  win.gotoPref("paneAdvanced");
  for (let element of elements) {
    if (element.nodeName == "preferences") {
      continue;
    }
    // The siteDataGroup in the Storage Management project will replace the offlineGroup eventually,
    // so during the transition period, we have to check the pref to see if the offlineGroup
    // should be hidden always. See the bug 1354530 for the details.
    if (element.id == "offlineGroup" && offlineGroupDisabled) {
      is_element_hidden(element, "Disabled offlineGroup should be hidden");
      continue;
    }
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneAdvanced") {
      is_element_visible(element, "Advanced elements should be visible");
    } else {
      is_element_hidden(element, "Non-Advanced elements should be hidden");
    }
  }

  gBrowser.removeCurrentTab();
  win.close();
  finish();
}
