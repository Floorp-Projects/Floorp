/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  gCertDB: ["@mozilla.org/security/x509certdb;1", "nsIX509CertDB"],
  gXulStore: ["@mozilla.org/xul/xulstore;1", "nsIXULStore"],
});

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BookmarksPolicies: "resource:///modules/policies/BookmarksPolicies.jsm",
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  ProxyPolicies: "resource:///modules/policies/ProxyPolicies.jsm",
  WebsiteFilter: "resource:///modules/policies/WebsiteFilter.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["File", "FileReader"]);

const PREF_LOGLEVEL = "browser.policies.loglevel";
const BROWSER_DOCUMENT_URL = AppConstants.BROWSER_CHROME_URL;

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
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
  "3rdparty": {
    onBeforeAddons(manager, param) {
      manager.setExtensionPolicies(param.Extensions);
    },
  },

  AppUpdateURL: {
    onBeforeAddons(manager, param) {
      setDefaultPref("app.update.url", param.href);
    },
  },

  Authentication: {
    onBeforeAddons(manager, param) {
      if ("SPNEGO" in param) {
        setAndLockPref(
          "network.negotiate-auth.trusted-uris",
          param.SPNEGO.join(", ")
        );
      }
      if ("Delegated" in param) {
        setAndLockPref(
          "network.negotiate-auth.delegation-uris",
          param.Delegated.join(", ")
        );
      }
      if ("NTLM" in param) {
        setAndLockPref(
          "network.automatic-ntlm-auth.trusted-uris",
          param.NTLM.join(", ")
        );
      }
      if ("AllowNonFQDN" in param) {
        if (param.AllowNonFQDN.NTLM) {
          setAndLockPref(
            "network.automatic-ntlm-auth.allow-non-fqdn",
            param.AllowNonFQDN.NTLM
          );
        }
        if (param.AllowNonFQDN.SPNEGO) {
          setAndLockPref(
            "network.negotiate-auth.allow-non-fqdn",
            param.AllowNonFQDN.SPNEGO
          );
        }
      }
    },
  },

  BlockAboutAddons: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        blockAboutPage(manager, "about:addons", true);
      }
    },
  },

  BlockAboutConfig: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        blockAboutPage(manager, "about:config");
        setAndLockPref("devtools.chrome.enabled", false);
      }
    },
  },

  BlockAboutProfiles: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        blockAboutPage(manager, "about:profiles");
      }
    },
  },

  BlockAboutSupport: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        blockAboutPage(manager, "about:support");
      }
    },
  },

  Bookmarks: {
    onAllWindowsRestored(manager, param) {
      BookmarksPolicies.processBookmarks(param);
    },
  },

  CaptivePortal: {
    onBeforeAddons(manager, param) {
      setAndLockPref("network.captive-portal-service.enabled", param);
    },
  },

  Certificates: {
    onBeforeAddons(manager, param) {
      if ("ImportEnterpriseRoots" in param) {
        setAndLockPref(
          "security.enterprise_roots.enabled",
          param.ImportEnterpriseRoots
        );
      }
      if ("Install" in param) {
        (async () => {
          let dirs = [];
          let platform = AppConstants.platform;
          if (platform == "win") {
            dirs = [
              // Ugly, but there is no official way to get %USERNAME\AppData\Roaming\Mozilla.
              Services.dirsvc.get("XREUSysExt", Ci.nsIFile).parent,
              // Even more ugly, but there is no official way to get %USERNAME\AppData\Local\Mozilla.
              Services.dirsvc.get("DefProfLRt", Ci.nsIFile).parent.parent,
            ];
          } else if (platform == "macosx" || platform == "linux") {
            dirs = [
              // These two keys are named wrong. They return the Mozilla directory.
              Services.dirsvc.get("XREUserNativeManifests", Ci.nsIFile),
              Services.dirsvc.get("XRESysNativeManifests", Ci.nsIFile),
            ];
          }
          dirs.unshift(Services.dirsvc.get("XREAppDist", Ci.nsIFile));
          for (let certfilename of param.Install) {
            let certfile;
            try {
              certfile = Cc["@mozilla.org/file/local;1"].createInstance(
                Ci.nsIFile
              );
              certfile.initWithPath(certfilename);
            } catch (e) {
              for (let dir of dirs) {
                certfile = dir.clone();
                certfile.append(
                  platform == "linux" ? "certificates" : "Certificates"
                );
                certfile.append(certfilename);
                if (certfile.exists()) {
                  break;
                }
              }
            }
            let file;
            try {
              file = await File.createFromNsIFile(certfile);
            } catch (e) {
              log.error(`Unable to find certificate - ${certfilename}`);
              continue;
            }
            let reader = new FileReader();
            reader.onloadend = function() {
              if (reader.readyState != reader.DONE) {
                log.error(`Unable to read certificate - ${certfile.path}`);
                return;
              }
              let certFile = reader.result;
              let cert;
              try {
                cert = gCertDB.constructX509(certFile);
              } catch (e) {
                try {
                  // It might be PEM instead of DER.
                  cert = gCertDB.constructX509FromBase64(pemToBase64(certFile));
                } catch (ex) {
                  log.error(`Unable to add certificate - ${certfile.path}`);
                }
              }
              let now = Date.now() / 1000;
              if (cert) {
                gCertDB.asyncVerifyCertAtTime(
                  cert,
                  0x0008 /* certificateUsageSSLCA */,
                  0,
                  null,
                  now,
                  (aPRErrorCode, aVerifiedChain, aHasEVPolicy) => {
                    if (aPRErrorCode == Cr.NS_OK) {
                      // Certificate is already installed.
                      return;
                    }
                    try {
                      gCertDB.addCert(certFile, "CT,CT,");
                    } catch (e) {
                      // It might be PEM instead of DER.
                      gCertDB.addCertFromBase64(
                        pemToBase64(certFile),
                        "CT,CT,"
                      );
                    }
                  }
                );
              }
            };
            reader.readAsBinaryString(file);
          }
        })();
      }
    },
  },

  Cookies: {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("cookie", param.Allow, param.Block);

      if (param.Block) {
        const hosts = param.Block.map(url => url.hostname)
          .sort()
          .join("\n");
        runOncePerModification("clearCookiesForBlockedHosts", hosts, () => {
          for (let blocked of param.Block) {
            Services.cookies.removeCookiesWithOriginAttributes(
              "{}",
              blocked.hostname
            );
          }
        });
      }

      if (
        param.Default !== undefined ||
        param.AcceptThirdParty !== undefined ||
        param.RejectTracker !== undefined ||
        param.Locked
      ) {
        const ACCEPT_COOKIES = 0;
        const REJECT_THIRD_PARTY_COOKIES = 1;
        const REJECT_ALL_COOKIES = 2;
        const REJECT_UNVISITED_THIRD_PARTY = 3;
        const REJECT_TRACKER = 4;

        let newCookieBehavior = ACCEPT_COOKIES;
        if (param.Default !== undefined && !param.Default) {
          newCookieBehavior = REJECT_ALL_COOKIES;
        } else if (param.AcceptThirdParty) {
          if (param.AcceptThirdParty == "never") {
            newCookieBehavior = REJECT_THIRD_PARTY_COOKIES;
          } else if (param.AcceptThirdParty == "from-visited") {
            newCookieBehavior = REJECT_UNVISITED_THIRD_PARTY;
          }
        } else if (param.RejectTracker !== undefined && param.RejectTracker) {
          newCookieBehavior = REJECT_TRACKER;
        }

        setDefaultPref(
          "network.cookie.cookieBehavior",
          newCookieBehavior,
          param.Locked
        );
      }

      const KEEP_COOKIES_UNTIL_EXPIRATION = 0;
      const KEEP_COOKIES_UNTIL_END_OF_SESSION = 2;

      if (param.ExpireAtSessionEnd !== undefined || param.Locked) {
        let newLifetimePolicy = KEEP_COOKIES_UNTIL_EXPIRATION;
        if (param.ExpireAtSessionEnd) {
          newLifetimePolicy = KEEP_COOKIES_UNTIL_END_OF_SESSION;
        }

        setDefaultPref(
          "network.cookie.lifetimePolicy",
          newLifetimePolicy,
          param.Locked
        );
      }
    },
  },

  DefaultDownloadDirectory: {
    onBeforeAddons(manager, param) {
      setDefaultPref("browser.download.dir", replacePathVariables(param));
      // If a custom download directory is being used, just lock folder list to 2.
      setAndLockPref("browser.download.folderList", 2);
    },
  },

  DisableAppUpdate: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("appUpdate");
      }
    },
  },

  DisableBuiltinPDFViewer: {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("pdfjs.disabled", true);
      }
    },
  },

  DisableDeveloperTools: {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("devtools.policy.disabled", true);
        setAndLockPref("devtools.chrome.enabled", false);

        manager.disallowFeature("devtools");
        blockAboutPage(manager, "about:devtools");
        blockAboutPage(manager, "about:debugging");
        blockAboutPage(manager, "about:devtools-toolbox");
      }
    },
  },

  DisableFeedbackCommands: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("feedbackCommands");
      }
    },
  },

  DisableFirefoxAccounts: {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("identity.fxaccounts.enabled", false);
      }
    },
  },

  DisableFirefoxScreenshots: {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("extensions.screenshots.disabled", true);
      }
    },
  },

  DisableFirefoxStudies: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("Shield");
        setAndLockPref(
          "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
          false
        );
        setAndLockPref(
          "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
          false
        );
      }
    },
  },

  DisableForgetButton: {
    onProfileAfterChange(manager, param) {
      if (param) {
        setAndLockPref("privacy.panicButton.enabled", false);
      }
    },
  },

  DisableFormHistory: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        setAndLockPref("browser.formfill.enable", false);
      }
    },
  },

  DisableMasterPasswordCreation: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("createMasterPassword");
      }
    },
  },

  DisablePocket: {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("extensions.pocket.enabled", false);
      }
    },
  },

  DisablePrivateBrowsing: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("privatebrowsing");
        blockAboutPage(manager, "about:privatebrowsing", true);
        setAndLockPref("browser.privatebrowsing.autostart", false);
      }
    },
  },

  DisableProfileImport: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("profileImport");
        setAndLockPref(
          "browser.newtabpage.activity-stream.migrationExpired",
          true
        );
      }
    },
  },

  DisableProfileRefresh: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("profileRefresh");
        setAndLockPref("browser.disableResetPrompt", true);
      }
    },
  },

  DisableSafeMode: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("safeMode");
      }
    },
  },

  DisableSecurityBypass: {
    onBeforeUIStartup(manager, param) {
      if ("InvalidCertificate" in param) {
        setAndLockPref(
          "security.certerror.hideAddException",
          param.InvalidCertificate
        );
      }

      if ("SafeBrowsing" in param) {
        setAndLockPref(
          "browser.safebrowsing.allowOverride",
          !param.SafeBrowsing
        );
      }
    },
  },

  DisableSetDesktopBackground: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("setDesktopBackground");
      }
    },
  },

  DisableSystemAddonUpdate: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("SysAddonUpdate");
      }
    },
  },

  DisableTelemetry: {
    onBeforeAddons(manager, param) {
      if (param) {
        setAndLockPref("datareporting.healthreport.uploadEnabled", false);
        setAndLockPref("datareporting.policy.dataSubmissionEnabled", false);
        blockAboutPage(manager, "about:telemetry");
      }
    },
  },

  DisplayBookmarksToolbar: {
    onBeforeUIStartup(manager, param) {
      let value = (!param).toString();
      // This policy is meant to change the default behavior, not to force it.
      // If this policy was alreay applied and the user chose to re-hide the
      // bookmarks toolbar, do not show it again.
      runOncePerModification("displayBookmarksToolbar", value, () => {
        gXulStore.setValue(
          BROWSER_DOCUMENT_URL,
          "PersonalToolbar",
          "collapsed",
          value
        );
      });
    },
  },

  DisplayMenuBar: {
    onBeforeUIStartup(manager, param) {
      let value = (!param).toString();
      // This policy is meant to change the default behavior, not to force it.
      // If this policy was alreay applied and the user chose to re-hide the
      // menu bar, do not show it again.
      runOncePerModification("displayMenuBar", value, () => {
        gXulStore.setValue(
          BROWSER_DOCUMENT_URL,
          "toolbar-menubar",
          "autohide",
          value
        );
      });
    },
  },

  DNSOverHTTPS: {
    onBeforeAddons(manager, param) {
      if ("Enabled" in param) {
        let mode = param.Enabled ? 2 : 5;
        setDefaultPref("network.trr.mode", mode, param.Locked);
      }
      if (param.ProviderURL) {
        setDefaultPref("network.trr.uri", param.ProviderURL.href, param.Locked);
      }
    },
  },

  DontCheckDefaultBrowser: {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("browser.shell.checkDefaultBrowser", !param);
    },
  },

  DownloadDirectory: {
    onBeforeAddons(manager, param) {
      setAndLockPref("browser.download.dir", replacePathVariables(param));
      // If a custom download directory is being used, just lock folder list to 2.
      setAndLockPref("browser.download.folderList", 2);
      // Per Chrome spec, user can't choose to download every time
      // if this is set.
      setAndLockPref("browser.download.useDownloadDir", true);
    },
  },

  EnableTrackingProtection: {
    onBeforeUIStartup(manager, param) {
      if (param.Value) {
        setDefaultPref(
          "privacy.trackingprotection.enabled",
          true,
          param.Locked
        );
        setDefaultPref(
          "privacy.trackingprotection.pbmode.enabled",
          true,
          param.Locked
        );
      } else {
        setAndLockPref("privacy.trackingprotection.enabled", false);
        setAndLockPref("privacy.trackingprotection.pbmode.enabled", false);
      }
    },
  },

  Extensions: {
    onBeforeUIStartup(manager, param) {
      let uninstallingPromise = Promise.resolve();
      if ("Uninstall" in param) {
        uninstallingPromise = runOncePerModification(
          "extensionsUninstall",
          JSON.stringify(param.Uninstall),
          async () => {
            // If we're uninstalling add-ons, re-run the extensionsInstall runOnce even if it hasn't
            // changed, which will allow add-ons to be updated.
            Services.prefs.clearUserPref(
              "browser.policies.runOncePerModification.extensionsInstall"
            );
            let addons = await AddonManager.getAddonsByIDs(param.Uninstall);
            for (let addon of addons) {
              if (addon) {
                try {
                  await addon.uninstall();
                } catch (e) {
                  // This can fail for add-ons that can't be uninstalled.
                  log.debug(`Add-on ID (${addon.id}) couldn't be uninstalled.`);
                }
              }
            }
          }
        );
      }
      if ("Install" in param) {
        runOncePerModification(
          "extensionsInstall",
          JSON.stringify(param.Install),
          async () => {
            await uninstallingPromise;
            for (let location of param.Install) {
              let uri;
              try {
                // We need to try as a file first because
                // Windows paths are valid URIs.
                // This is done for legacy support (old API)
                let xpiFile = new FileUtils.File(location);
                uri = Services.io.newFileURI(xpiFile);
              } catch (e) {
                uri = Services.io.newURI(location);
              }
              installAddonFromURL(uri.spec);
            }
          }
        );
      }
      if ("Locked" in param) {
        for (let ID of param.Locked) {
          manager.disallowFeature(`uninstall-extension:${ID}`);
          manager.disallowFeature(`disable-extension:${ID}`);
        }
      }
    },
  },

  ExtensionSettings: {
    onBeforeAddons(manager, param) {
      try {
        manager.setExtensionSettings(param);
      } catch (e) {
        log.error("Invalid ExtensionSettings");
      }
    },
    async onBeforeUIStartup(manager, param) {
      let extensionSettings = param;
      let blockAllExtensions = false;
      if ("*" in extensionSettings) {
        if (
          "installation_mode" in extensionSettings["*"] &&
          extensionSettings["*"].installation_mode == "blocked"
        ) {
          blockAllExtensions = true;
          // Turn off discovery pane in about:addons
          setAndLockPref("extensions.getAddons.showPane", false);
          // Block about:debugging
          blockAboutPage(manager, "about:debugging");
        }
      }
      let { addons } = await AddonManager.getActiveAddons();
      let allowedExtensions = [];
      for (let extensionID in extensionSettings) {
        if (extensionID == "*") {
          // Ignore global settings
          continue;
        }
        if ("installation_mode" in extensionSettings[extensionID]) {
          if (
            extensionSettings[extensionID].installation_mode ==
              "force_installed" ||
            extensionSettings[extensionID].installation_mode ==
              "normal_installed"
          ) {
            if (!extensionSettings[extensionID].install_url) {
              throw new Error(`Missing install_url for ${extensionID}`);
            }
            if (!addons.find(addon => addon.id == extensionID)) {
              installAddonFromURL(
                extensionSettings[extensionID].install_url,
                extensionID
              );
            }
            manager.disallowFeature(`uninstall-extension:${extensionID}`);
            if (
              extensionSettings[extensionID].installation_mode ==
              "force_installed"
            ) {
              manager.disallowFeature(`disable-extension:${extensionID}`);
            }
            allowedExtensions.push(extensionID);
          } else if (
            extensionSettings[extensionID].installation_mode == "allowed"
          ) {
            allowedExtensions.push(extensionID);
          } else if (
            extensionSettings[extensionID].installation_mode == "blocked"
          ) {
            if (addons.find(addon => addon.id == extensionID)) {
              // Can't use the addon from getActiveAddons since it doesn't have uninstall.
              let addon = await AddonManager.getAddonByID(extensionID);
              try {
                await addon.uninstall();
              } catch (e) {
                // This can fail for add-ons that can't be uninstalled.
                log.debug(`Add-on ID (${addon.id}) couldn't be uninstalled.`);
              }
            }
          }
        }
      }
      if (blockAllExtensions) {
        for (let addon of addons) {
          if (
            addon.isSystem ||
            addon.isBuiltin ||
            !(addon.scope & AddonManager.SCOPE_PROFILE)
          ) {
            continue;
          }
          if (!allowedExtensions.includes(addon.id)) {
            try {
              // Can't use the addon from getActiveAddons since it doesn't have uninstall.
              let addonToUninstall = await AddonManager.getAddonByID(addon.id);
              await addonToUninstall.uninstall();
            } catch (e) {
              // This can fail for add-ons that can't be uninstalled.
              log.debug(`Add-on ID (${addon.id}) couldn't be uninstalled.`);
            }
          }
        }
      }
    },
  },

  ExtensionUpdate: {
    onBeforeAddons(manager, param) {
      if (!param) {
        setAndLockPref("extensions.update.enabled", param);
      }
    },
  },

  FirefoxHome: {
    onBeforeAddons(manager, param) {
      let locked = param.Locked || false;
      if ("Search" in param) {
        setDefaultPref(
          "browser.newtabpage.activity-stream.showSearch",
          param.Search,
          locked
        );
      }
      if ("TopSites" in param) {
        setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.topsites",
          param.TopSites,
          locked
        );
      }
      if ("Highlights" in param) {
        setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.section.highlights",
          param.Highlights,
          locked
        );
      }
      if ("Pocket" in param) {
        setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.section.topstories",
          param.Pocket,
          locked
        );
      }
      if ("Snippets" in param) {
        setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.snippets",
          param.Snippets,
          locked
        );
      }
    },
  },

  FlashPlugin: {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("plugin:flash", param.Allow, param.Block);

      const FLASH_NEVER_ACTIVATE = 0;
      const FLASH_ASK_TO_ACTIVATE = 1;

      let flashPrefVal;
      if (param.Default === undefined || param.Default) {
        flashPrefVal = FLASH_ASK_TO_ACTIVATE;
      } else {
        flashPrefVal = FLASH_NEVER_ACTIVATE;
      }
      if (param.Locked) {
        setAndLockPref("plugin.state.flash", flashPrefVal);
      } else if (param.Default !== undefined) {
        setDefaultPref("plugin.state.flash", flashPrefVal);
      }
    },
  },

  HardwareAcceleration: {
    onBeforeAddons(manager, param) {
      if (!param) {
        setAndLockPref("layers.acceleration.disabled", true);
      }
    },
  },

  Homepage: {
    onBeforeUIStartup(manager, param) {
      // |homepages| will be a string containing a pipe-separated ('|') list of
      // URLs because that is what the "Home page" section of about:preferences
      // (and therefore what the pref |browser.startup.homepage|) accepts.
      if (param.URL) {
        let homepages = param.URL.href;
        if (param.Additional && param.Additional.length > 0) {
          homepages += "|" + param.Additional.map(url => url.href).join("|");
        }
        setDefaultPref("browser.startup.homepage", homepages, param.Locked);
        if (param.Locked) {
          setAndLockPref(
            "pref.browser.homepage.disable_button.current_page",
            true
          );
          setAndLockPref(
            "pref.browser.homepage.disable_button.bookmark_page",
            true
          );
          setAndLockPref(
            "pref.browser.homepage.disable_button.restore_default",
            true
          );
        } else {
          // Clear out old run once modification that is no longer used.
          clearRunOnceModification("setHomepage");
        }
      }
      if (param.StartPage) {
        let prefValue;
        switch (param.StartPage) {
          case "none":
            prefValue = 0;
            break;
          case "homepage":
            prefValue = 1;
            break;
          case "previous-session":
            prefValue = 3;
            break;
        }
        setDefaultPref("browser.startup.page", prefValue, param.Locked);
      }
    },
  },

  InstallAddonsPermission: {
    onBeforeUIStartup(manager, param) {
      if ("Allow" in param) {
        addAllowDenyPermissions("install", param.Allow, null);
      }
      if ("Default" in param) {
        setAndLockPref("xpinstall.enabled", param.Default);
        if (!param.Default) {
          blockAboutPage(manager, "about:debugging");
          setAndLockPref(
            "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
            false
          );
          setAndLockPref(
            "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
            false
          );
          manager.disallowFeature("xpinstall");
        }
      }
    },
  },

  LocalFileLinks: {
    onBeforeAddons(manager, param) {
      // If there are existing capabilities, lock them with the policy pref.
      let policyNames = Services.prefs
        .getCharPref("capability.policy.policynames", "")
        .split(" ");
      policyNames.push("localfilelinks_policy");
      setAndLockPref("capability.policy.policynames", policyNames.join(" "));
      setAndLockPref(
        "capability.policy.localfilelinks_policy.checkloaduri.enabled",
        "allAccess"
      );
      setAndLockPref(
        "capability.policy.localfilelinks_policy.sites",
        param.join(" ")
      );
    },
  },

  NetworkPrediction: {
    onBeforeAddons(manager, param) {
      setAndLockPref("network.dns.disablePrefetch", !param);
      setAndLockPref("network.dns.disablePrefetchFromHTTPS", !param);
    },
  },

  NewTabPage: {
    onBeforeAddons(manager, param) {
      setAndLockPref("browser.newtabpage.enabled", param);
    },
  },

  NoDefaultBookmarks: {
    onProfileAfterChange(manager, param) {
      if (param) {
        manager.disallowFeature("defaultBookmarks");
      }
    },
  },

  OfferToSaveLogins: {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("signon.rememberSignons", param);
    },
  },

  OverrideFirstRunPage: {
    onProfileAfterChange(manager, param) {
      let url = param ? param.href : "";
      setAndLockPref("startup.homepage_welcome_url", url);
    },
  },

  OverridePostUpdatePage: {
    onProfileAfterChange(manager, param) {
      let url = param ? param.href : "";
      setAndLockPref("startup.homepage_override_url", url);
      // The pref startup.homepage_override_url is only used
      // as a fallback when the update.xml file hasn't provided
      // a specific post-update URL.
      manager.disallowFeature("postUpdateCustomPage");
    },
  },

  Permissions: {
    onBeforeUIStartup(manager, param) {
      if (param.Camera) {
        addAllowDenyPermissions(
          "camera",
          param.Camera.Allow,
          param.Camera.Block
        );
        setDefaultPermission("camera", param.Camera);
      }

      if (param.Microphone) {
        addAllowDenyPermissions(
          "microphone",
          param.Microphone.Allow,
          param.Microphone.Block
        );
        setDefaultPermission("microphone", param.Microphone);
      }

      if (param.Location) {
        addAllowDenyPermissions(
          "geo",
          param.Location.Allow,
          param.Location.Block
        );
        setDefaultPermission("geo", param.Location);
      }

      if (param.Notifications) {
        addAllowDenyPermissions(
          "desktop-notification",
          param.Notifications.Allow,
          param.Notifications.Block
        );
        setDefaultPermission("desktop-notification", param.Notifications);
      }
    },
  },

  PopupBlocking: {
    onBeforeUIStartup(manager, param) {
      addAllowDenyPermissions("popup", param.Allow, null);

      if (param.Locked) {
        let blockValue = true;
        if (param.Default !== undefined && !param.Default) {
          blockValue = false;
        }
        setAndLockPref("dom.disable_open_during_load", blockValue);
      } else if (param.Default !== undefined) {
        setDefaultPref("dom.disable_open_during_load", !!param.Default);
      }
    },
  },

  Preferences: {
    onBeforeAddons(manager, param) {
      for (let preference in param) {
        setAndLockPref(preference, param[preference]);
      }
    },
  },

  PromptForDownloadLocation: {
    onBeforeAddons(manager, param) {
      setAndLockPref("browser.download.useDownloadDir", !param);
    },
  },

  Proxy: {
    onBeforeAddons(manager, param) {
      if (param.Locked) {
        manager.disallowFeature("changeProxySettings");
        ProxyPolicies.configureProxySettings(param, setAndLockPref);
      } else {
        ProxyPolicies.configureProxySettings(param, setDefaultPref);
      }
    },
  },

  RequestedLocales: {
    onBeforeAddons(manager, param) {
      if (Array.isArray(param)) {
        Services.locale.requestedLocales = param;
      } else {
        Services.locale.requestedLocales = param.split(",");
      }
    },
  },

  SanitizeOnShutdown: {
    onBeforeUIStartup(manager, param) {
      if (typeof param === "boolean") {
        setAndLockPref("privacy.sanitize.sanitizeOnShutdown", param);
        if (param) {
          setAndLockPref("privacy.clearOnShutdown.cache", true);
          setAndLockPref("privacy.clearOnShutdown.cookies", true);
          setAndLockPref("privacy.clearOnShutdown.downloads", true);
          setAndLockPref("privacy.clearOnShutdown.formdata", true);
          setAndLockPref("privacy.clearOnShutdown.history", true);
          setAndLockPref("privacy.clearOnShutdown.sessions", true);
          setAndLockPref("privacy.clearOnShutdown.siteSettings", true);
          setAndLockPref("privacy.clearOnShutdown.offlineApps", true);
        }
      } else {
        setAndLockPref("privacy.sanitize.sanitizeOnShutdown", true);
        if ("Cache" in param) {
          setAndLockPref("privacy.clearOnShutdown.cache", param.Cache);
        } else {
          setAndLockPref("privacy.clearOnShutdown.cache", false);
        }
        if ("Cookies" in param) {
          setAndLockPref("privacy.clearOnShutdown.cookies", param.Cookies);
        } else {
          setAndLockPref("privacy.clearOnShutdown.cookies", false);
        }
        if ("Downloads" in param) {
          setAndLockPref("privacy.clearOnShutdown.downloads", param.Downloads);
        } else {
          setAndLockPref("privacy.clearOnShutdown.downloads", false);
        }
        if ("FormData" in param) {
          setAndLockPref("privacy.clearOnShutdown.formdata", param.FormData);
        } else {
          setAndLockPref("privacy.clearOnShutdown.formdata", false);
        }
        if ("History" in param) {
          setAndLockPref("privacy.clearOnShutdown.history", param.History);
        } else {
          setAndLockPref("privacy.clearOnShutdown.history", false);
        }
        if ("Sessions" in param) {
          setAndLockPref("privacy.clearOnShutdown.sessions", param.Sessions);
        } else {
          setAndLockPref("privacy.clearOnShutdown.sessions", false);
        }
        if ("SiteSettings" in param) {
          setAndLockPref(
            "privacy.clearOnShutdown.siteSettings",
            param.SiteSettings
          );
        }
        if ("OfflineApps" in param) {
          setAndLockPref(
            "privacy.clearOnShutdown.offlineApps",
            param.OfflineApps
          );
        }
      }
    },
  },

  SearchBar: {
    onAllWindowsRestored(manager, param) {
      // This policy is meant to change the default behavior, not to force it.
      // If this policy was already applied and the user chose move the search
      // bar, don't move it again.
      runOncePerModification("searchInNavBar", param, () => {
        if (param == "separate") {
          CustomizableUI.addWidgetToArea(
            "search-container",
            CustomizableUI.AREA_NAVBAR,
            CustomizableUI.getPlacementOfWidget("urlbar-container").position + 1
          );
        } else if (param == "unified") {
          CustomizableUI.removeWidgetFromArea("search-container");
        }
      });
    },
  },

  SearchEngines: {
    onBeforeUIStartup(manager, param) {
      if (param.PreventInstalls) {
        manager.disallowFeature("installSearchEngine", true);
      }
    },
    onAllWindowsRestored(manager, param) {
      Services.search.init().then(async () => {
        if (param.Remove) {
          // Only rerun if the list of engine names has changed.
          await runOncePerModification(
            "removeSearchEngines",
            JSON.stringify(param.Remove),
            async function() {
              for (let engineName of param.Remove) {
                let engine = Services.search.getEngineByName(engineName);
                if (engine) {
                  try {
                    await Services.search.removeEngine(engine);
                  } catch (ex) {
                    log.error("Unable to remove the search engine", ex);
                  }
                }
              }
            }
          );
        }
        if (param.Add) {
          // Only rerun if the list of engine names has changed.
          let engineNameList = param.Add.map(engine => engine.Name);
          await runOncePerModification(
            "addSearchEngines",
            JSON.stringify(engineNameList),
            async function() {
              for (let newEngine of param.Add) {
                let newEngineParameters = {
                  template: newEngine.URLTemplate,
                  iconURL: newEngine.IconURL ? newEngine.IconURL.href : null,
                  alias: newEngine.Alias,
                  description: newEngine.Description,
                  method: newEngine.Method,
                  postData: newEngine.PostData,
                  suggestURL: newEngine.SuggestURLTemplate,
                  extensionID: "set-via-policy",
                  queryCharset: "UTF-8",
                };
                try {
                  await Services.search.addEngineWithDetails(
                    newEngine.Name,
                    newEngineParameters
                  );
                } catch (ex) {
                  log.error("Unable to add search engine", ex);
                }
              }
            }
          );
        }
        if (param.Default) {
          await runOncePerModification(
            "setDefaultSearchEngine",
            param.Default,
            async () => {
              let defaultEngine;
              try {
                defaultEngine = Services.search.getEngineByName(param.Default);
                if (!defaultEngine) {
                  throw new Error("No engine by that name could be found");
                }
              } catch (ex) {
                log.error(
                  `Search engine lookup failed when attempting to set ` +
                    `the default engine. Requested engine was ` +
                    `"${param.Default}".`,
                  ex
                );
              }
              if (defaultEngine) {
                try {
                  await Services.search.setDefault(defaultEngine);
                } catch (ex) {
                  log.error("Unable to set the default search engine", ex);
                }
              }
            }
          );
        }
      });
    },
  },

  SearchSuggestEnabled: {
    onBeforeAddons(manager, param) {
      setAndLockPref("browser.urlbar.suggest.searches", param);
      setAndLockPref("browser.search.suggest.enabled", param);
    },
  },

  SecurityDevices: {
    onProfileAfterChange(manager, param) {
      let securityDevices = param;
      let pkcs11db = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(
        Ci.nsIPKCS11ModuleDB
      );
      let moduleList = pkcs11db.listModules();
      for (let deviceName in securityDevices) {
        let foundModule = false;
        for (let module of moduleList) {
          if (module && module.libName === securityDevices[deviceName]) {
            foundModule = true;
            break;
          }
        }
        if (foundModule) {
          continue;
        }
        try {
          pkcs11db.addModule(deviceName, securityDevices[deviceName], 0, 0);
        } catch (ex) {
          log.error(`Unable to add security device ${deviceName}`);
          log.debug(ex);
        }
      }
    },
  },

  SSLVersionMax: {
    onBeforeAddons(manager, param) {
      let tlsVersion;
      switch (param) {
        case "tls1":
          tlsVersion = 1;
          break;
        case "tls1.1":
          tlsVersion = 2;
          break;
        case "tls1.2":
          tlsVersion = 3;
          break;
        case "tls1.3":
          tlsVersion = 4;
          break;
      }
      setAndLockPref("security.tls.version.max", tlsVersion);
    },
  },

  SSLVersionMin: {
    onBeforeAddons(manager, param) {
      let tlsVersion;
      switch (param) {
        case "tls1":
          tlsVersion = 1;
          break;
        case "tls1.1":
          tlsVersion = 2;
          break;
        case "tls1.2":
          tlsVersion = 3;
          break;
        case "tls1.3":
          tlsVersion = 4;
          break;
      }
      setAndLockPref("security.tls.version.min", tlsVersion);
    },
  },

  SupportMenu: {
    onProfileAfterChange(manager, param) {
      manager.setSupportMenu(param);
    },
  },

  WebsiteFilter: {
    onBeforeUIStartup(manager, param) {
      this.filter = new WebsiteFilter(
        param.Block || [],
        param.Exceptions || []
      );
    },
  },
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
  setDefaultPref(prefName, prefValue, true);
}

/**
 * setDefaultPref
 *
 * Sets the _default_ value of a pref and optionally locks it.
 * The value is only changed in memory, and not stored to disk.
 *
 * @param {string} prefName
 *        The pref to be changed
 * @param {boolean,number,string} prefValue
 *        The value to set
 * @param {boolean} locked
 *        Optionally lock the pref
 */
function setDefaultPref(prefName, prefValue, locked = false) {
  if (Services.prefs.prefIsLocked(prefName)) {
    Services.prefs.unlockPref(prefName);
  }

  let defaults = Services.prefs.getDefaultBranch("");

  switch (typeof prefValue) {
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

  if (locked) {
    Services.prefs.lockPref(prefName);
  }
}

/**
 * setDefaultPermission
 *
 * Helper function to set preferences appropriately for the policy
 *
 * @param {string} policyName
 *        The name of the policy to set
 * @param {object} policyParam
 *        The object containing param for the policy
 */
function setDefaultPermission(policyName, policyParam) {
  if ("BlockNewRequests" in policyParam) {
    let prefName = "permissions.default." + policyName;

    if (policyParam.BlockNewRequests) {
      setDefaultPref(prefName, 2, policyParam.Locked);
    } else {
      setDefaultPref(prefName, 0, policyParam.Locked);
    }
  }
}

/**
 * addAllowDenyPermissions
 *
 * Helper function to call the permissions manager (Services.perms.addFromPrincipal)
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
    try {
      Services.perms.addFromPrincipal(
        Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin),
        permissionName,
        Ci.nsIPermissionManager.ALLOW_ACTION,
        Ci.nsIPermissionManager.EXPIRE_POLICY
      );
    } catch (ex) {
      log.error(`Added by default for ${permissionName} permission in the permission
      manager - ${origin.href}`);
    }
  }

  for (let origin of blockList) {
    Services.perms.addFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin),
      permissionName,
      Ci.nsIPermissionManager.DENY_ACTION,
      Ci.nsIPermissionManager.EXPIRE_POLICY
    );
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
    log.debug(
      `Not running action ${actionName} again because it has already run.`
    );
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
 * If the callback that was passed is an async function, you can await on this
 * function to await for the callback.
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
 * @returns Promise
 *        A promise that will resolve once the callback finishes running.
 *
 */
async function runOncePerModification(actionName, policyValue, callback) {
  let prefName = `browser.policies.runOncePerModification.${actionName}`;
  let oldPolicyValue = Services.prefs.getStringPref(prefName, undefined);
  if (policyValue === oldPolicyValue) {
    log.debug(
      `Not running action ${actionName} again because the policy's value is unchanged`
    );
    return Promise.resolve();
  }
  Services.prefs.setStringPref(prefName, policyValue);
  return callback();
}

/**
 * clearRunOnceModification
 *
 * Helper function that clears a runOnce policy.
 */
function clearRunOnceModification(actionName) {
  let prefName = `browser.policies.runOncePerModification.${actionName}`;
  Services.prefs.clearUserPref(prefName);
}

function replacePathVariables(path) {
  if (path.includes("${home}")) {
    return path.replace("${home}", FileUtils.getFile("Home", []).path);
  }
  return path;
}

/**
 * installAddonFromURL
 *
 * Helper function that installs an addon from a URL
 * and verifies that the addon ID matches.
 */
function installAddonFromURL(url, extensionID) {
  AddonManager.getInstallForURL(url, {
    telemetryInfo: { source: "enterprise-policy" },
  }).then(install => {
    if (install.addon && install.addon.appDisabled) {
      log.error(`Incompatible add-on - ${location}`);
      install.cancel();
      return;
    }
    let listener = {
      /* eslint-disable-next-line no-shadow */
      onDownloadEnded: install => {
        if (extensionID && install.addon.id != extensionID) {
          log.error(
            `Add-on downloaded from ${url} had unexpected id (got ${
              install.addon.id
            } expected ${extensionID})`
          );
          install.removeListener(listener);
          install.cancel();
        }
        if (install.addon && install.addon.appDisabled) {
          log.error(`Incompatible add-on - ${url}`);
          install.removeListener(listener);
          install.cancel();
        }
      },
      onDownloadFailed: () => {
        install.removeListener(listener);
        log.error(
          `Download failed - ${AddonManager.errorToString(
            install.error
          )} - ${url}`
        );
        clearRunOnceModification("extensionsInstall");
      },
      onInstallFailed: () => {
        install.removeListener(listener);
        log.error(
          `Installation failed - ${AddonManager.errorToString(
            install.error
          )} - {url}`
        );
      },
      onInstallEnded: () => {
        install.removeListener(listener);
        log.debug(`Installation succeeded - ${url}`);
      },
    };
    install.addListener(listener);
    install.install();
  });
}

let gBlockedChromePages = [];

function blockAboutPage(manager, feature, neededOnContentProcess = false) {
  if (!gBlockedChromePages.length) {
    addChromeURLBlocker();
  }
  manager.disallowFeature(feature, neededOnContentProcess);
  let splitURL = Services.io
    .newChannelFromURI(
      Services.io.newURI(feature),
      null,
      Services.scriptSecurityManager.getSystemPrincipal(),
      null,
      0,
      Ci.nsIContentPolicy.TYPE_OTHER
    )
    .URI.spec.split("/");
  // about:debugging uses index.html for a filename, so we need to rely
  // on more than just the filename.
  let fileName =
    splitURL[splitURL.length - 2] + "/" + splitURL[splitURL.length - 1];
  gBlockedChromePages.push(fileName);
  if (feature == "about:config") {
    // Hide old page until it is removed
    gBlockedChromePages.push("config.xul");
  }
}

let ChromeURLBlockPolicy = {
  shouldLoad(contentLocation, loadInfo, mimeTypeGuess) {
    let contentType = loadInfo.externalContentPolicyType;
    if (
      contentLocation.scheme == "chrome" &&
      contentType == Ci.nsIContentPolicy.TYPE_DOCUMENT &&
      loadInfo.loadingContext &&
      loadInfo.loadingContext.baseURI == AppConstants.BROWSER_CHROME_URL &&
      gBlockedChromePages.some(function(fileName) {
        return contentLocation.filePath.endsWith(fileName);
      })
    ) {
      return Ci.nsIContentPolicy.REJECT_REQUEST;
    }
    return Ci.nsIContentPolicy.ACCEPT;
  },
  shouldProcess(contentLocation, loadInfo, mimeTypeGuess) {
    return Ci.nsIContentPolicy.ACCEPT;
  },
  classDescription: "Policy Engine Content Policy",
  contractID: "@mozilla-org/policy-engine-content-policy-service;1",
  classID: Components.ID("{ba7b9118-cabc-4845-8b26-4215d2a59ed7}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPolicy]),
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  },
};

function addChromeURLBlocker() {
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(
    ChromeURLBlockPolicy.classID,
    ChromeURLBlockPolicy.classDescription,
    ChromeURLBlockPolicy.contractID,
    ChromeURLBlockPolicy
  );

  Services.catMan.addCategoryEntry(
    "content-policy",
    ChromeURLBlockPolicy.contractID,
    ChromeURLBlockPolicy.contractID,
    false,
    true
  );
}

function pemToBase64(pem) {
  return pem
    .replace(/-----BEGIN CERTIFICATE-----/, "")
    .replace(/-----END CERTIFICATE-----/, "")
    .replace(/[\r\n]/g, "");
}
