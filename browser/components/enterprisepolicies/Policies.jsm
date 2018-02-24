/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gXulStore",
                                   "@mozilla.org/xul/xulstore;1",
                                   "nsIXULStore");

XPCOMUtils.defineLazyModuleGetters(this, {
  BookmarksPolicies: "resource:///modules/policies/BookmarksPolicies.jsm",
});

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

var EXPORTED_SYMBOLS = ["Policies"];

var Policies = {
  "BlockAboutAddons": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("about:addons", true);
      }
    }
  },

  "BlockAboutConfig": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("about:config", true);
      }
    }
  },

  "BlockAboutProfiles": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("about:profiles", true);
      }
    }
  },

  "BlockAboutSupport": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("about:support", true);
      }
    }
  },

  "BlockSetDesktopBackground": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("setDesktopBackground", true);
      }
    }
  },

  "Bookmarks": {
    onAllWindowsRestored(manager, param) {
      BookmarksPolicies.processBookmarks(param);
    }
  },

  "Cookies": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("cookie", param.Allow, param.Block);
    }
  },

  "CreateMasterPassword": {
    onBeforeUIStartup(manager, param) {
      if (!param) {
        manager.disallowFeature("createMasterPassword");
      }
    }
  },

  "DisableAppUpdate": {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("appUpdate");
      }
    }
  },

  "DisableFirefoxScreenshots": {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("extensions.screenshots.disabled", true);
      }
    }
  },

  "DisableFirefoxStudies": {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("Shield");
      }
    }
  },

  "DisableFormHistory": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        setAndLockPref("browser.formfill.enable", false);
      }
    }
  },

  "DisablePocket": {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("extensions.pocket.enabled", false);
      }
    }
  },

  "DisplayBookmarksToolbar": {
    onBeforeUIStartup(manager, param) {
      if (param) {
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

  "DisplayMenuBar": {
    onBeforeUIStartup(manager, param) {
      if (param) {
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

  "DontCheckDefaultBrowser": {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("browser.shell.checkDefaultBrowser", false);
    }
  },

  "FlashPlugin": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("plugin:flash", param.Allow, param.Block);
    }
  },

  "InstallAddons": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("install", param.Allow, param.Block);
    }
  },

  "Popups": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("popup", param.Allow, param.Block);
    }
  },

  "RememberPasswords": {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("signon.rememberSignons", param);
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
