/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */


function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    // Reset pref to its default
    Services.prefs.clearUserPref("browser.preferences.inContent");
  });
  
  // Verify that about:preferences tab is displayed when
  // browser.preferences.inContent is set to true
  Services.prefs.setBoolPref("browser.preferences.inContent", true);

  // Open a new tab.
  whenNewTabLoaded(window, testPreferences);
}

function testPreferences() {
  whenTabLoaded(gBrowser.selectedTab, function () {
    is(Services.prefs.getBoolPref("browser.preferences.inContent"), true, "In-content prefs are enabled");
    is(content.location.href, "about:preferences", "Checking if the preferences tab was opened");

    gBrowser.removeCurrentTab();
    Services.prefs.setBoolPref("browser.preferences.inContent", false);
    openPreferences();
  });
  
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        windowWatcher.unregisterNotification(observer);
        
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function() {
          win.removeEventListener("load", arguments.callee, false);
          is(Services.prefs.getBoolPref("browser.preferences.inContent"), false, "In-content prefs are disabled");
          is(win.location.href, "chrome://browser/content/preferences/preferences.xul", "Checking if the preferences window was opened");
          win.close();
          finish();
        }, false);
      }
    }
  }
  
  var windowWatcher = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                        .getService(Components.interfaces.nsIWindowWatcher);
  windowWatcher.registerNotification(observer);
  
  openPreferences();
}
