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
  ProxyPolicies: "resource:///modules/policies/ProxyPolicies.jsm",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
});

const PREF_LOGLEVEL           = "browser.policies.loglevel";
const BROWSER_DOCUMENT_URL    = "chrome://browser/content/browser.xul";

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

/*
 * ============================
 * = POLICIES IMPLEMENTATIONS =
 * ============================
 *
 * The Policies object below is where the implementation for each policy
 * happens. An object for each policy should be defined, containing
 * callback functions that will be called by the engine.
 *
 * See the _callbacks object in EnterprisePolicies.js for the list of
 * possible callbacks and an explanation of each.
 *
 * Each callback will be called with two parameters:
 * - manager
 *   This is the EnterprisePoliciesManager singleton object from
 *   EnterprisePolicies.js
 *
 * - param
 *   The parameter defined for this policy in policies-schema.json.
 *   It will be different for each policy. It could be a boolean,
 *   a string, an array or a complex object. All parameters have
 *   been validated according to the schema, and no unknown
 *   properties will be present on them.
 *
 * The callbacks will be bound to their parent policy object.
 */
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
        setAndLockPref("devtools.chrome.enabled", false);
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

      if (param.Block) {
        const hosts = param.Block.map(uri => uri.host).sort().join("\n");
        runOncePerModification("clearCookiesForBlockedHosts", hosts, () => {
          for (let blocked of param.Block) {
            Services.cookies.removeCookiesWithOriginAttributes("{}", blocked.host);
          }
        });
      }
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

  "DisableBuiltinPDFViewer": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("PDF.js");
      }
    }
  },

  "DisableDeveloperTools": {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("devtools.policy.disabled", true);
        setAndLockPref("devtools.chrome.enabled", false);

        manager.disallowFeature("devtools");
        manager.disallowFeature("about:devtools");
        manager.disallowFeature("about:debugging");
        manager.disallowFeature("about:devtools-toolbox");
      }
    }
  },

  "DisableFeedbackCommands": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("feedbackCommands");
      }
    }
  },

  "DisableFirefoxAccounts": {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("identity.fxaccounts.enabled", false);
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

  "DisablePrivateBrowsing": {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("privatebrowsing");
        manager.disallowFeature("about:privatebrowsing", true);
        setAndLockPref("browser.privatebrowsing.autostart", false);
      }
    }
  },

  "DisableSafeMode": {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("safeMode");
      }
    }
  },

  "DisableSysAddonUpdate": {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("SysAddonUpdate");
      }
    }
  },

  "DisplayBookmarksToolbar": {
    onBeforeUIStartup(manager, param) {
      let value = (!param).toString();
      // This policy is meant to change the default behavior, not to force it.
      // If this policy was alreay applied and the user chose to re-hide the
      // bookmarks toolbar, do not show it again.
      runOncePerModification("displayBookmarksToolbar", value, () => {
        gXulStore.setValue(BROWSER_DOCUMENT_URL, "PersonalToolbar", "collapsed", value);
      });
    }
  },

  "DisplayMenuBar": {
    onBeforeUIStartup(manager, param) {
      let value = (!param).toString();
        // This policy is meant to change the default behavior, not to force it.
        // If this policy was alreay applied and the user chose to re-hide the
        // menu bar, do not show it again.
      runOncePerModification("displayMenuBar", value, () => {
        gXulStore.setValue(BROWSER_DOCUMENT_URL, "toolbar-menubar", "autohide", value);
      });
    }
  },

  "DontCheckDefaultBrowser": {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("browser.shell.checkDefaultBrowser", false);
    }
  },

  "EnableTrackingProtection": {
    onBeforeUIStartup(manager, param) {
      if (param.Value) {
        if (param.Locked) {
          setAndLockPref("privacy.trackingprotection.enabled", true);
          setAndLockPref("privacy.trackingprotection.pbmode.enabled", true);
        } else {
          setDefaultPref("privacy.trackingprotection.enabled", true);
          setDefaultPref("privacy.trackingprotection.pbmode.enabled", true);
        }
      } else {
        setAndLockPref("privacy.trackingprotection.enabled", false);
        setAndLockPref("privacy.trackingprotection.pbmode.enabled", false);
      }
    }
  },

  "Extensions": {
    onBeforeUIStartup(manager, param) {
      if ("Install" in param) {
        runOncePerModification("extensionsInstall", JSON.stringify(param.Install), () => {
          for (let location of param.Install) {
            let url;
            if (location.includes("://")) {
              // Assume location is an URI
              url = location;
            } else {
              // Assume location is a file path
              let xpiFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
              try {
                xpiFile.initWithPath(location);
              } catch (e) {
                log.error(`Invalid extension path location - ${location}`);
                continue;
              }
              url = Services.io.newFileURI(xpiFile).spec;
            }
            AddonManager.getInstallForURL(url, (install) => {
              let listener = {
                onDownloadFailed: () => {
                  log.error(`Download failed - ${location}`);
                },
                onInstallFailed: () => {
                  log.error(`Installation failed - ${location}`);
                },
                onInstallEnded: () => {
                  log.debug(`Installation succeeded - ${location}`);
                }
              };
              install.addListener(listener);
              install.install();
            }, "application/x-xpinstall");
          }
        });
      }
      if ("Uninstall" in param) {
        runOncePerModification("extensionsUninstall", JSON.stringify(param.Uninstall), () => {
          AddonManager.getAddonsByIDs(param.Uninstall, (addons) => {
            for (let addon of addons) {
              if (addon) {
                addon.uninstall();
              }
            }
          });
        });
      }
      if ("Locked" in param) {
        for (let ID of param.Locked) {
          manager.disallowFeature(`modify-extension:${ID}`);
        }
      }
    }
  },

  "FlashPlugin": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("plugin:flash", param.Allow, param.Block);
    }
  },

  "Homepage": {
    onBeforeUIStartup(manager, param) {
      // |homepages| will be a string containing a pipe-separated ('|') list of
      // URLs because that is what the "Home page" section of about:preferences
      // (and therefore what the pref |browser.startup.homepage|) accepts.
      let homepages = param.URL.spec;
      if (param.Additional && param.Additional.length > 0) {
        homepages += "|" + param.Additional.map(url => url.spec).join("|");
      }
      if (param.Locked) {
        setAndLockPref("browser.startup.homepage", homepages);
        setAndLockPref("browser.startup.page", 1);
        setAndLockPref("pref.browser.homepage.disable_button.current_page", true);
        setAndLockPref("pref.browser.homepage.disable_button.bookmark_page", true);
        setAndLockPref("pref.browser.homepage.disable_button.restore_default", true);
      } else {
        // The default pref for homepage is actually a complex pref. We need to
        // set it in a special way such that it works properly
        let homepagePrefVal = "data:text/plain,browser.startup.homepage=" +
                               homepages;
        setDefaultPref("browser.startup.homepage", homepagePrefVal);
        setDefaultPref("browser.startup.page", 1);
        runOncePerModification("setHomepage", homepages, () => {
          Services.prefs.clearUserPref("browser.startup.homepage");
          Services.prefs.clearUserPref("browser.startup.page");
        });
      }
    }
  },

  "InstallAddons": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("install", param.Allow, null);
    }
  },

  "NoDefaultBookmarks": {
    onProfileAfterChange(manager, param) {
      if (param) {
        manager.disallowFeature("defaultBookmarks");
      }
    }
  },

  "Popups": {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("popup", param.Allow, null);
    }
  },

  "Proxy": {
    onBeforeAddons(manager, param) {
      if (param.Locked) {
        manager.disallowFeature("changeProxySettings");
        ProxyPolicies.configureProxySettings(param, setAndLockPref);
      } else {
        ProxyPolicies.configureProxySettings(param, setDefaultPref);
      }
    }
  },

  "RememberPasswords": {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("signon.rememberSignons", param);
    }
  },

  "SearchEngines": {
    onBeforeUIStartup(manager, param) {
      if (param.PreventInstalls) {
        manager.disallowFeature("installSearchEngine", true);
      }
    },
    onAllWindowsRestored(manager, param) {
      Services.search.init(() => {
        if (param.Add) {
          // Only rerun if the list of engine names has changed.
          let engineNameList = param.Add.map(engine => engine.Name);
          runOncePerModification("addSearchEngines",
                                 JSON.stringify(engineNameList),
                                 () => {
            for (let newEngine of param.Add) {
              let newEngineParameters = {
                template:    newEngine.URLTemplate,
                iconURL:     newEngine.IconURL,
                alias:       newEngine.Alias,
                description: newEngine.Description,
                method:      newEngine.Method,
                suggestURL:  newEngine.SuggestURLTemplate,
                extensionID: "set-via-policy"
              };
              try {
                Services.search.addEngineWithDetails(newEngine.Name,
                                                     newEngineParameters);
              } catch (ex) {
                log.error("Unable to add search engine", ex);
              }
            }
          });
        }
        if (param.Default) {
          runOnce("setDefaultSearchEngine", () => {
            let defaultEngine;
            try {
              defaultEngine = Services.search.getEngineByName(param.Default);
              if (!defaultEngine) {
                throw "No engine by that name could be found";
              }
            } catch (ex) {
              log.error(`Search engine lookup failed when attempting to set ` +
                        `the default engine. Requested engine was ` +
                        `"${param.Default}".`, ex);
            }
            if (defaultEngine) {
              try {
                Services.search.currentEngine = defaultEngine;
              } catch (ex) {
                log.error("Unable to set the default search engine", ex);
              }
            }
          });
        }
      });
    }
  }
};

/*
 * ====================
 * = HELPER FUNCTIONS =
 * ====================
 *
 * The functions below are helpers to be used by several policies.
 */

/**
 * setAndLockPref
 *
 * Sets the _default_ value of a pref, and locks it (meaning that
 * the default value will always be returned, independent from what
 * is stored as the user value).
 * The value is only changed in memory, and not stored to disk.
 *
 * @param {string} prefName
 *        The pref to be changed
 * @param {boolean,number,string} prefValue
 *        The value to set and lock
 */
function setAndLockPref(prefName, prefValue) {
  if (Services.prefs.prefIsLocked(prefName)) {
    Services.prefs.unlockPref(prefName);
  }

  setDefaultPref(prefName, prefValue);

  Services.prefs.lockPref(prefName);
}

/**
 * setDefaultPref
 *
 * Sets the _default_ value of a pref.
 * The value is only changed in memory, and not stored to disk.
 *
 * @param {string} prefName
 *        The pref to be changed
 * @param {boolean,number,string} prefValue
 *        The value to set
 */
function setDefaultPref(prefName, prefValue) {
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
}

/**
 * addAllowDenyPermissions
 *
 * Helper function to call the permissions manager (Services.perms.add)
 * for two arrays of URLs.
 *
 * @param {string} permissionName
 *        The name of the permission to change
 * @param {array} allowList
 *        The list of URLs to be set as ALLOW_ACTION for the chosen permission.
 * @param {array} blockList
 *        The list of URLs to be set as DENY_ACTION for the chosen permission.
 */
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

/**
 * runOnce
 *
 * Helper function to run a callback only once per policy.
 *
 * @param {string} actionName
 *        A given name which will be used to track if this callback has run.
 * @param {Functon} callback
 *        The callback to run only once.
 */
 // eslint-disable-next-line no-unused-vars
function runOnce(actionName, callback) {
  let prefName = `browser.policies.runonce.${actionName}`;
  if (Services.prefs.getBoolPref(prefName, false)) {
    log.debug(`Not running action ${actionName} again because it has already run.`);
    return;
  }
  Services.prefs.setBoolPref(prefName, true);
  callback();
}

/**
 * runOncePerModification
 *
 * Helper function similar to runOnce. The difference is that runOnce runs the
 * callback once when the policy is set, then never again.
 * runOncePerModification runs the callback once each time the policy value
 * changes from its previous value.
 *
 * @param {string} actionName
 *        A given name which will be used to track if this callback has run.
 *        This string will be part of a pref name.
 * @param {string} policyValue
 *        The current value of the policy. This will be compared to previous
 *        values given to this function to determine if the policy value has
 *        changed. Regardless of the data type of the policy, this must be a
 *        string.
 * @param {Function} callback
 *        The callback to be run when the pref value changes
 */
function runOncePerModification(actionName, policyValue, callback) {
  let prefName = `browser.policies.runOncePerModification.${actionName}`;
  let oldPolicyValue = Services.prefs.getStringPref(prefName, undefined);
  if (policyValue === oldPolicyValue) {
    log.debug(`Not running action ${actionName} again because the policy's value is unchanged`);
    return;
  }
  Services.prefs.setStringPref(prefName, policyValue);
  callback();
}
