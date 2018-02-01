/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gXulStore",
                                   "@mozilla.org/xul/xulstore;1",
                                   "nsIXULStore");

const PREF_LOGLEVEL           = "browser.policies.loglevel";
const PREF_MENU_ALREADY_DISPLAYED = "browser.policies.menuBarWasDisplayed";
const BROWSER_DOCUMENT_URL        = "chrome://browser/content/browser.xul";
const PREF_BOOKMARKS_ALREADY_DISPLAYED = "browser.policies.bookmarkBarWasDisplayed";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    prefix: "Policies.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

this.EXPORTED_SYMBOLS = ["Policies"];

this.Policies = {
  "block_about_config": {
    onBeforeUIStartup(manager, param) {
      if (param == true) {
        manager.disallowFeature("about:config", true);
      }
    }
  },

  "display_menu_bar": {
    onBeforeUIStartup(manager, param) {
      if (param == true) {
        // This policy is meant to change the default behavior, not to force it.
        // If this policy was alreay applied and the user chose to re-hide the
        // menu bar, do not show it again.
        if (!Services.prefs.getBoolPref(PREF_MENU_ALREADY_DISPLAYED, false)) {
          log.debug("Showing the menu bar");
          gXulStore.setValue(BROWSER_DOCUMENT_URL, "toolbar-menubar", "autohide", "false");
          Services.prefs.setBoolPref(PREF_MENU_ALREADY_DISPLAYED, true);
        } else {
          log.debug("Not showing the menu bar because it has already been shown.");
        }
      }
    }
  },

  "display_bookmarks_toolbar": {
    onBeforeUIStartup(manager, param) {
      if (param == true) {
        // This policy is meant to change the default behavior, not to force it.
        // If this policy was alreay applied and the user chose to re-hide the
        // bookmarks toolbar, do not show it again.
        if (!Services.prefs.getBoolPref(PREF_BOOKMARKS_ALREADY_DISPLAYED, false)) {
          log.debug("Showing the bookmarks toolbar");
          gXulStore.setValue(BROWSER_DOCUMENT_URL, "PersonalToolbar", "collapsed", "false");
          Services.prefs.setBoolPref(PREF_BOOKMARKS_ALREADY_DISPLAYED, true);
        } else {
          log.debug("Not showing the bookmarks toolbar because it has already been shown.");
        }
      }
    }
  },

  "block_set_desktop_background": {
    onBeforeUIStartup(manager, param) {
      if (param == true) {
        manager.disallowFeature("setDesktopBackground", true);
      }
    }
  },

  "DisableFirefoxScreenshots": {
    onBeforeAddons(manager, param) {
      if (param == true) {
        setAndLockPref("extensions.screenshots.disabled", true);
      }
    }
  },

  "dont_check_default_browser": {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("browser.shell.checkDefaultBrowser", false);
    }
  },

  "flash_plugin": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("plugin:flash", param.allow, param.block);
    }
  },

  "popups": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("popup", param.allow, param.block);
    }
  },

  "install_addons": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("install", param.allow, param.block);
    }
  },

  "cookies": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("cookie", param.allow, param.block);
    }
  },
};

/*
 * ====================
 * = HELPER FUNCTIONS =
 * ====================
 *
 * The functions below are helpers to be used by several policies.
 */

function setAndLockPref(prefName, prefValue) {
  if (Services.prefs.prefIsLocked(prefName)) {
    Services.prefs.unlockPref(prefName);
  }

  let defaults = Services.prefs.getDefaultBranch("");

  switch (typeof(prefValue)) {
    case "boolean":
      defaults.setBoolPref(prefName, prefValue);
      break;

    case "number":
      if (!Number.isInteger(prefValue)) {
        throw new Error(`Non-integer value for ${prefName}`);
      }

      defaults.setIntPref(prefName, prefValue);
      break;

    case "string":
      defaults.setStringPref(prefName, prefValue);
      break;
  }

  Services.prefs.lockPref(prefName);
}

function addAllowDenyPermissions(permissionName, allowList, blockList) {
  allowList = allowList || [];
  blockList = blockList || [];

  for (let origin of allowList) {
    Services.perms.add(origin,
                       permissionName,
                       Ci.nsIPermissionManager.ALLOW_ACTION,
                       Ci.nsIPermissionManager.EXPIRE_POLICY);
  }

  for (let origin of blockList) {
    Services.perms.add(origin,
                       permissionName,
                       Ci.nsIPermissionManager.DENY_ACTION,
                       Ci.nsIPermissionManager.EXPIRE_POLICY);
  }
}
