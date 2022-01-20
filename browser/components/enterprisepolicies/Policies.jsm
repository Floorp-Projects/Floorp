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
  gExternalProtocolService: [
    "@mozilla.org/uriloader/external-protocol-service;1",
    "nsIExternalProtocolService",
  ],
  gHandlerService: [
    "@mozilla.org/uriloader/handler-service;1",
    "nsIHandlerService",
  ],
  gMIMEService: ["@mozilla.org/mime;1", "nsIMIMEService"],
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
const ABOUT_CONTRACT = "@mozilla.org/network/protocol/about;1?what=";

let env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);
const isXpcshell = env.exists("XPCSHELL_TEST_PROFILE_DIR");

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

var EXPORTED_SYMBOLS = [
  "Policies",
  "setAndLockPref",
  "PoliciesUtils",
  "runOnce",
];

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
  // Used for cleaning up policies.
  // Use the same timing that you used for setting up the policy.
  _cleanup: {
    onBeforeAddons(manager) {
      if (Cu.isInAutomation || isXpcshell) {
        console.log("_cleanup from onBeforeAddons");
        clearBlockedAboutPages();
      }
    },
    onProfileAfterChange(manager) {
      if (Cu.isInAutomation || isXpcshell) {
        console.log("_cleanup from onProfileAfterChange");
      }
    },
    onBeforeUIStartup(manager) {
      if (Cu.isInAutomation || isXpcshell) {
        console.log("_cleanup from onBeforeUIStartup");
      }
    },
    onAllWindowsRestored(manager) {
      if (Cu.isInAutomation || isXpcshell) {
        console.log("_cleanup from onAllWindowsRestored");
      }
    },
  },

  "3rdparty": {
    onBeforeAddons(manager, param) {
      manager.setExtensionPolicies(param.Extensions);
    },
  },

  AllowedDomainsForApps: {
    onBeforeAddons(manager, param) {
      Services.obs.addObserver(function(subject, topic, data) {
        let channel = subject.QueryInterface(Ci.nsIHttpChannel);
        if (channel.URI.host.endsWith(".google.com")) {
          channel.setRequestHeader("X-GoogApps-Allowed-Domains", param, true);
        }
      }, "http-on-modify-request");
    },
  },

  AppAutoUpdate: {
    onBeforeUIStartup(manager, param) {
      // Logic feels a bit reversed here, but it's correct. If AppAutoUpdate is
      // true, we disallow turning off auto updating, and visa versa.
      if (param) {
        manager.disallowFeature("app-auto-updates-off");
      } else {
        manager.disallowFeature("app-auto-updates-on");
      }
    },
  },

  AppUpdateURL: {
    // No implementation needed here. UpdateService.jsm will check for this
    // policy directly when determining the update URL.
  },

  Authentication: {
    onBeforeAddons(manager, param) {
      let locked = true;
      if ("Locked" in param) {
        locked = param.Locked;
      }

      if ("SPNEGO" in param) {
        PoliciesUtils.setDefaultPref(
          "network.negotiate-auth.trusted-uris",
          param.SPNEGO.join(", "),
          locked
        );
      }
      if ("Delegated" in param) {
        PoliciesUtils.setDefaultPref(
          "network.negotiate-auth.delegation-uris",
          param.Delegated.join(", "),
          locked
        );
      }
      if ("NTLM" in param) {
        PoliciesUtils.setDefaultPref(
          "network.automatic-ntlm-auth.trusted-uris",
          param.NTLM.join(", "),
          locked
        );
      }
      if ("AllowNonFQDN" in param) {
        if ("NTLM" in param.AllowNonFQDN) {
          PoliciesUtils.setDefaultPref(
            "network.automatic-ntlm-auth.allow-non-fqdn",
            param.AllowNonFQDN.NTLM,
            locked
          );
        }
        if ("SPNEGO" in param.AllowNonFQDN) {
          PoliciesUtils.setDefaultPref(
            "network.negotiate-auth.allow-non-fqdn",
            param.AllowNonFQDN.SPNEGO,
            locked
          );
        }
      }
      if ("AllowProxies" in param) {
        if ("NTLM" in param.AllowProxies) {
          PoliciesUtils.setDefaultPref(
            "network.automatic-ntlm-auth.allow-proxies",
            param.AllowProxies.NTLM,
            locked
          );
        }
        if ("SPNEGO" in param.AllowProxies) {
          PoliciesUtils.setDefaultPref(
            "network.negotiate-auth.allow-proxies",
            param.AllowProxies.SPNEGO,
            locked
          );
        }
      }
      if ("PrivateBrowsing" in param) {
        PoliciesUtils.setDefaultPref(
          "network.auth.private-browsing-sso",
          param.PrivateBrowsing,
          locked
        );
      }
    },
  },

  AutoLaunchProtocolsFromOrigins: {
    onBeforeAddons(manager, param) {
      for (let info of param) {
        addAllowDenyPermissions(
          `open-protocol-handler^${info.protocol}`,
          info.allowed_origins
        );
      }
    },
  },

  BackgroundAppUpdate: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("app-background-update-off");
      } else {
        manager.disallowFeature("app-background-update-on");
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
              let certFileArray = [];
              for (let i = 0; i < certFile.length; i++) {
                certFileArray.push(certFile.charCodeAt(i));
              }
              let cert;
              try {
                cert = gCertDB.constructX509(certFileArray);
              } catch (e) {
                log.debug(
                  `constructX509 failed with error '${e}' - trying constructX509FromBase64.`
                );
                try {
                  // It might be PEM instead of DER.
                  cert = gCertDB.constructX509FromBase64(pemToBase64(certFile));
                } catch (ex) {
                  log.error(`Unable to add certificate - ${certfile.path}`, ex);
                }
              }
              if (cert) {
                if (
                  gCertDB.isCertTrusted(
                    cert,
                    Ci.nsIX509Cert.CA_CERT,
                    Ci.nsIX509CertDB.TRUSTED_SSL
                  )
                ) {
                  // Certificate is already installed.
                  return;
                }
                try {
                  gCertDB.addCert(certFile, "CT,CT,");
                } catch (e) {
                  // It might be PEM instead of DER.
                  gCertDB.addCertFromBase64(pemToBase64(certFile), "CT,CT,");
                }
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

      if (param.AllowSession) {
        for (let origin of param.AllowSession) {
          try {
            Services.perms.addFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                origin
              ),
              "cookie",
              Ci.nsICookiePermission.ACCESS_SESSION,
              Ci.nsIPermissionManager.EXPIRE_POLICY
            );
          } catch (ex) {
            log.error(
              `Unable to add cookie session permission - ${origin.href}`
            );
          }
        }
      }

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

      if (param.ExpireAtSessionEnd !== undefined || param.Locked) {
        let newLifetimePolicy = Ci.nsICookieService.ACCEPT_NORMALLY;
        if (param.ExpireAtSessionEnd) {
          newLifetimePolicy = Ci.nsICookieService.ACCEPT_SESSION;
        }

        PoliciesUtils.setDefaultPref(
          "network.cookie.lifetimePolicy",
          newLifetimePolicy,
          param.Locked
        );
      }

      // New Cookie Behavior option takes precendence
      let defaultPref = Services.prefs.getDefaultBranch("");
      let newCookieBehavior = defaultPref.getIntPref(
        "network.cookie.cookieBehavior"
      );
      let newCookieBehaviorPB = defaultPref.getIntPref(
        "network.cookie.cookieBehavior.pbmode"
      );
      if ("Behavior" in param || "BehaviorPrivateBrowsing" in param) {
        let behaviors = {
          accept: Ci.nsICookieService.BEHAVIOR_ACCEPT,
          "reject-foreign": Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
          reject: Ci.nsICookieService.BEHAVIOR_REJECT,
          "limit-foreign": Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
          "reject-tracker": Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
          "reject-tracker-and-partition-foreign":
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
        };
        if ("Behavior" in param) {
          newCookieBehavior = behaviors[param.Behavior];
        }
        if ("BehaviorPrivateBrowsing" in param) {
          newCookieBehaviorPB = behaviors[param.BehaviorPrivateBrowsing];
        }
      } else {
        // Default, AcceptThirdParty, and RejectTracker are being
        // deprecated in favor of Behavior. They will continue
        // to be supported, though.
        if (
          param.Default !== undefined ||
          param.AcceptThirdParty !== undefined ||
          param.RejectTracker !== undefined ||
          param.Locked
        ) {
          newCookieBehavior = Ci.nsICookieService.BEHAVIOR_ACCEPT;
          if (param.Default !== undefined && !param.Default) {
            newCookieBehavior = Ci.nsICookieService.BEHAVIOR_REJECT;
          } else if (param.AcceptThirdParty) {
            if (param.AcceptThirdParty == "never") {
              newCookieBehavior = Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
            } else if (param.AcceptThirdParty == "from-visited") {
              newCookieBehavior = Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
            }
          } else if (param.RejectTracker) {
            newCookieBehavior = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          }
        }
        // With the old cookie policy, we made private browsing the same.
        newCookieBehaviorPB = newCookieBehavior;
      }
      // We set the values no matter what just in case the policy was only used to lock.
      PoliciesUtils.setDefaultPref(
        "network.cookie.cookieBehavior",
        newCookieBehavior,
        param.Locked
      );
      PoliciesUtils.setDefaultPref(
        "network.cookie.cookieBehavior.pbmode",
        newCookieBehaviorPB,
        param.Locked
      );
    },
  },

  DefaultDownloadDirectory: {
    onBeforeAddons(manager, param) {
      PoliciesUtils.setDefaultPref(
        "browser.download.dir",
        replacePathVariables(param)
      );
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

  DisabledCiphers: {
    onBeforeAddons(manager, param) {
      if ("TLS_DHE_RSA_WITH_AES_128_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.dhe_rsa_aes_128_sha",
          !param.TLS_DHE_RSA_WITH_AES_128_CBC_SHA
        );
      }
      if ("TLS_DHE_RSA_WITH_AES_256_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.dhe_rsa_aes_256_sha",
          !param.TLS_DHE_RSA_WITH_AES_256_CBC_SHA
        );
      }
      if ("TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.ecdhe_rsa_aes_128_sha",
          !param.TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
        );
      }
      if ("TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.ecdhe_rsa_aes_256_sha",
          !param.TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
        );
      }
      if ("TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256" in param) {
        setAndLockPref(
          "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
          !param.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
        );
      }
      if ("TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256" in param) {
        setAndLockPref(
          "security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256",
          !param.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
        );
      }
      if ("TLS_RSA_WITH_AES_128_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.rsa_aes_128_sha",
          !param.TLS_RSA_WITH_AES_128_CBC_SHA
        );
      }
      if ("TLS_RSA_WITH_AES_256_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.rsa_aes_256_sha",
          !param.TLS_RSA_WITH_AES_256_CBC_SHA
        );
      }
      if ("TLS_RSA_WITH_3DES_EDE_CBC_SHA" in param) {
        setAndLockPref(
          "security.ssl3.deprecated.rsa_des_ede3_sha",
          !param.TLS_RSA_WITH_3DES_EDE_CBC_SHA
        );
      }
      if ("TLS_RSA_WITH_AES_128_GCM_SHA256" in param) {
        setAndLockPref(
          "security.ssl3.rsa_aes_128_gcm_sha256",
          !param.TLS_RSA_WITH_AES_128_GCM_SHA256
        );
      }
      if ("TLS_RSA_WITH_AES_256_GCM_SHA384" in param) {
        setAndLockPref(
          "security.ssl3.rsa_aes_256_gcm_sha384",
          !param.TLS_RSA_WITH_AES_256_GCM_SHA384
        );
      }
    },
  },

  DisableDefaultBrowserAgent: {
    // The implementation of this policy is in the default browser agent itself
    // (/toolkit/mozapps/defaultagent); we need an entry for it here so that it
    // shows up in about:policies as a real policy and not as an error.
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
        blockAboutPage(manager, "about:profiling");
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
        setAndLockPref("browser.aboutwelcome.enabled", false);
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

  DisablePasswordReveal: {
    onBeforeUIStartup(manager, param) {
      if (param) {
        manager.disallowFeature("passwordReveal");
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
        setAndLockPref("toolkit.telemetry.archive.enabled", false);
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
        // Set the preference to keep the bookmarks bar open and also
        // declaratively open the bookmarks toolbar. Otherwise, default
        // to showing it on the New Tab Page.
        let visibilityPref = "browser.toolbars.bookmarks.visibility";
        let visibility = param ? "always" : "newtab";
        Services.prefs.setCharPref(visibilityPref, visibility);
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
      let value;
      if (
        typeof param === "boolean" ||
        param == "default-on" ||
        param == "default-off"
      ) {
        switch (param) {
          case "default-on":
            value = "false";
            break;
          case "default-off":
            value = "true";
            break;
          default:
            value = (!param).toString();
            break;
        }
        // This policy is meant to change the default behavior, not to force it.
        // If this policy was already applied and the user chose to re-hide the
        // menu bar, do not show it again.
        runOncePerModification("displayMenuBar", value, () => {
          gXulStore.setValue(
            BROWSER_DOCUMENT_URL,
            "toolbar-menubar",
            "autohide",
            value
          );
        });
      } else {
        switch (param) {
          case "always":
            value = "false";
            break;
          case "never":
            // Make sure Alt key doesn't show the menubar
            setAndLockPref("ui.key.menuAccessKeyFocuses", false);
            value = "true";
            break;
        }
        gXulStore.setValue(
          BROWSER_DOCUMENT_URL,
          "toolbar-menubar",
          "autohide",
          value
        );
        manager.disallowFeature("hideShowMenuBar");
      }
    },
  },

  DNSOverHTTPS: {
    onBeforeAddons(manager, param) {
      let locked = false;
      if ("Locked" in param) {
        locked = param.Locked;
      }
      if ("Enabled" in param) {
        let mode = param.Enabled ? 2 : 5;
        PoliciesUtils.setDefaultPref("network.trr.mode", mode, locked);
      }
      if ("ProviderURL" in param) {
        PoliciesUtils.setDefaultPref(
          "network.trr.uri",
          param.ProviderURL.href,
          locked
        );
      }
      if ("ExcludedDomains" in param) {
        PoliciesUtils.setDefaultPref(
          "network.trr.excluded-domains",
          param.ExcludedDomains.join(","),
          locked
        );
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
        PoliciesUtils.setDefaultPref(
          "privacy.trackingprotection.enabled",
          true,
          param.Locked
        );
        PoliciesUtils.setDefaultPref(
          "privacy.trackingprotection.pbmode.enabled",
          true,
          param.Locked
        );
      } else {
        setAndLockPref("privacy.trackingprotection.enabled", false);
        setAndLockPref("privacy.trackingprotection.pbmode.enabled", false);
      }
      if ("Cryptomining" in param) {
        PoliciesUtils.setDefaultPref(
          "privacy.trackingprotection.cryptomining.enabled",
          param.Cryptomining,
          param.Locked
        );
      }
      if ("Fingerprinting" in param) {
        PoliciesUtils.setDefaultPref(
          "privacy.trackingprotection.fingerprinting.enabled",
          param.Fingerprinting,
          param.Locked
        );
      }
      if ("Exceptions" in param) {
        addAllowDenyPermissions("trackingprotection", param.Exceptions);
      }
    },
  },

  EncryptedMediaExtensions: {
    onBeforeAddons(manager, param) {
      let locked = false;
      if ("Locked" in param) {
        locked = param.Locked;
      }
      if ("Enabled" in param) {
        PoliciesUtils.setDefaultPref(
          "media.eme.enabled",
          param.Enabled,
          locked
        );
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
          // Turn off recommendations
          setAndLockPref(
            "extensions.htmlaboutaddons.recommendations.enable",
            false
          );
          // Block about:debugging
          blockAboutPage(manager, "about:debugging");
        }
        if ("restricted_domains" in extensionSettings["*"]) {
          let restrictedDomains = Services.prefs
            .getCharPref("extensions.webextensions.restrictedDomains")
            .split(",");
          setAndLockPref(
            "extensions.webextensions.restrictedDomains",
            restrictedDomains
              .concat(extensionSettings["*"].restricted_domains)
              .join(",")
          );
        }
      }
      let addons = await AddonManager.getAllAddons();
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
            installAddonFromURL(
              extensionSettings[extensionID].install_url,
              extensionID,
              addons.find(addon => addon.id == extensionID)
            );
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
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.showSearch",
          param.Search,
          locked
        );
      }
      if ("TopSites" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.topsites",
          param.TopSites,
          locked
        );
      }
      if ("SponsoredTopSites" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.showSponsoredTopSites",
          param.SponsoredTopSites,
          locked
        );
      }
      if ("Highlights" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.section.highlights",
          param.Highlights,
          locked
        );
      }
      if ("Pocket" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.system.topstories",
          param.Pocket,
          locked
        );
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.feeds.section.topstories",
          param.Pocket,
          locked
        );
      }
      if ("SponsoredPocket" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.showSponsored",
          param.SponsoredPocket,
          locked
        );
      }
      if ("Snippets" in param) {
        PoliciesUtils.setDefaultPref(
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
        PoliciesUtils.setDefaultPref("plugin.state.flash", flashPrefVal);
      }
    },
  },

  Handlers: {
    onBeforeAddons(manager, param) {
      if ("mimeTypes" in param) {
        for (let mimeType in param.mimeTypes) {
          let mimeInfo = param.mimeTypes[mimeType];
          let realMIMEInfo = gMIMEService.getFromTypeAndExtension(mimeType, "");
          processMIMEInfo(mimeInfo, realMIMEInfo);
        }
      }
      if ("extensions" in param) {
        for (let extension in param.extensions) {
          let mimeInfo = param.extensions[extension];
          try {
            let realMIMEInfo = gMIMEService.getFromTypeAndExtension(
              "",
              extension
            );
            processMIMEInfo(mimeInfo, realMIMEInfo);
          } catch (e) {
            log.error(`Invalid file extension (${extension})`);
          }
        }
      }
      if ("schemes" in param) {
        for (let scheme in param.schemes) {
          let handlerInfo = param.schemes[scheme];
          let realHandlerInfo = gExternalProtocolService.getProtocolHandlerInfo(
            scheme
          );
          processMIMEInfo(handlerInfo, realHandlerInfo);
        }
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
      if ("StartPage" in param && param.StartPage == "none") {
        // For blank startpage, we use about:blank rather
        // than messing with browser.startup.page
        param.URL = new URL("about:blank");
      }
      // |homepages| will be a string containing a pipe-separated ('|') list of
      // URLs because that is what the "Home page" section of about:preferences
      // (and therefore what the pref |browser.startup.homepage|) accepts.
      if ("URL" in param) {
        let homepages = param.URL.href;
        if (param.Additional && param.Additional.length) {
          homepages += "|" + param.Additional.map(url => url.href).join("|");
        }
        PoliciesUtils.setDefaultPref(
          "browser.startup.homepage",
          homepages,
          param.Locked
        );
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
        // If a homepage has been set via policy, show the home button
        if (param.URL != "about:blank") {
          manager.disallowFeature("removeHomeButtonByDefault");
        }
      }
      if (param.StartPage) {
        let prefValue;
        switch (param.StartPage) {
          case "homepage":
          case "homepage-locked":
          case "none":
            prefValue = 1;
            break;
          case "previous-session":
            prefValue = 3;
            break;
        }
        PoliciesUtils.setDefaultPref(
          "browser.startup.page",
          prefValue,
          param.StartPage == "homepage-locked"
        );
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

  LegacyProfiles: {
    // Handled in nsToolkitProfileService.cpp (Windows only)
  },

  LegacySameSiteCookieBehaviorEnabled: {
    onBeforeAddons(manager, param) {
      PoliciesUtils.setDefaultPref(
        "network.cookie.sameSite.laxByDefault",
        !param
      );
    },
  },

  LegacySameSiteCookieBehaviorEnabledForDomainList: {
    onBeforeAddons(manager, param) {
      PoliciesUtils.setDefaultPref(
        "network.cookie.sameSite.laxByDefault.disabledHosts",
        param.join(",")
      );
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

  ManagedBookmarks: {},

  ManualAppUpdateOnly: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("autoAppUpdateChecking");
      }
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
      setAndLockPref("services.passwordSavingEnabled", param);
    },
  },

  OfferToSaveLoginsDefault: {
    onBeforeUIStartup(manager, param) {
      let policies = Services.policies.getActivePolicies();
      if ("OfferToSaveLogins" in policies) {
        log.error(
          `OfferToSaveLoginsDefault ignored because OfferToSaveLogins is present.`
        );
      } else {
        PoliciesUtils.setDefaultPref("signon.rememberSignons", param);
      }
    },
  },

  OverrideFirstRunPage: {
    onProfileAfterChange(manager, param) {
      let url = param ? param : "";
      setAndLockPref("startup.homepage_welcome_url", url);
      setAndLockPref("browser.aboutwelcome.enabled", false);
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

  PasswordManagerEnabled: {
    onBeforeUIStartup(manager, param) {
      if (!param) {
        blockAboutPage(manager, "about:logins", true);
        setAndLockPref("pref.privacy.disable_button.view_passwords", true);
      }
      setAndLockPref("signon.rememberSignons", param);
    },
  },

  PDFjs: {
    onBeforeAddons(manager, param) {
      if ("Enabled" in param) {
        setAndLockPref("pdfjs.disabled", !param.Enabled);
      }
      if ("EnablePermissions" in param) {
        setAndLockPref("pdfjs.enablePermissions", !param.Enabled);
      }
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

      if (param.Autoplay) {
        addAllowDenyPermissions(
          "autoplay-media",
          param.Autoplay.Allow,
          param.Autoplay.Block
        );
        if ("Default" in param.Autoplay) {
          let prefValue;
          switch (param.Autoplay.Default) {
            case "allow-audio-video":
              prefValue = 0;
              break;
            case "block-audio":
              prefValue = 1;
              break;
            case "block-audio-video":
              prefValue = 5;
              break;
          }
          PoliciesUtils.setDefaultPref(
            "media.autoplay.default",
            prefValue,
            param.Autoplay.Locked
          );
        }
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

      if ("VirtualReality" in param) {
        addAllowDenyPermissions(
          "xr",
          param.VirtualReality.Allow,
          param.VirtualReality.Block
        );
        setDefaultPermission("xr", param.VirtualReality);
      }
    },
  },

  PictureInPicture: {
    onBeforeAddons(manager, param) {
      if ("Enabled" in param) {
        PoliciesUtils.setDefaultPref(
          "media.videocontrols.picture-in-picture.video-toggle.enabled",
          param.Enabled
        );
      }
      if (param.Locked) {
        Services.prefs.lockPref(
          "media.videocontrols.picture-in-picture.video-toggle.enabled"
        );
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
        PoliciesUtils.setDefaultPref(
          "dom.disable_open_during_load",
          !!param.Default
        );
      }
    },
  },

  Preferences: {
    onBeforeAddons(manager, param) {
      const allowedPrefixes = [
        "accessibility.",
        "app.update.",
        "browser.",
        "datareporting.policy.",
        "dom.",
        "extensions.",
        "general.autoScroll",
        "general.smoothScroll",
        "geo.",
        "gfx.",
        "intl.",
        "keyword.enabled",
        "layers.",
        "layout.",
        "media.",
        "network.",
        "pdfjs.",
        "places.",
        "print.",
        "signon.",
        "spellchecker.",
        "toolkit.legacyUserProfileCustomizations.stylesheets",
        "ui.",
        "widget.",
      ];
      const allowedSecurityPrefs = [
        "security.default_personal_cert",
        "security.insecure_connection_text.enabled",
        "security.insecure_connection_text.pbmode.enabled",
        "security.insecure_field_warning.contextual.enabled",
        "security.mixed_content.block_active_content",
        "security.osclientcerts.autoload",
        "security.ssl.errorReporting.enabled",
        "security.tls.enable_0rtt_data",
        "security.tls.hello_downgrade_check",
        "security.tls.version.enable-deprecated",
        "security.warn_submit_secure_to_insecure",
      ];
      const blockedPrefs = [
        "app.update.channel",
        "app.update.lastUpdateTime",
        "app.update.migrated",
      ];

      for (let preference in param) {
        if (blockedPrefs.includes(preference)) {
          log.error(
            `Unable to set preference ${preference}. Preference not allowed for security reasons.`
          );
          continue;
        }
        if (preference.startsWith("security.")) {
          if (!allowedSecurityPrefs.includes(preference)) {
            log.error(
              `Unable to set preference ${preference}. Preference not allowed for security reasons.`
            );
            continue;
          }
        } else if (
          !allowedPrefixes.some(prefix => preference.startsWith(prefix))
        ) {
          log.error(
            `Unable to set preference ${preference}. Preference not allowed for stability reasons.`
          );
          continue;
        }
        if (typeof param[preference] != "object") {
          // Legacy policy preferences
          setAndLockPref(preference, param[preference]);
        } else {
          if (param[preference].Status == "clear") {
            Services.prefs.clearUserPref(preference);
            continue;
          }

          if (param[preference].Status == "user") {
            var prefBranch = Services.prefs;
          } else {
            prefBranch = Services.prefs.getDefaultBranch("");
          }

          try {
            switch (typeof param[preference].Value) {
              case "boolean":
                prefBranch.setBoolPref(preference, param[preference].Value);
                break;

              case "number":
                if (!Number.isInteger(param[preference].Value)) {
                  throw new Error(`Non-integer value for ${preference}`);
                }

                // This is ugly, but necessary. On Windows GPO and macOS
                // configs, booleans are converted to 0/1. In the previous
                // Preferences implementation, the schema took care of
                // automatically converting these values to booleans.
                // Since we allow arbitrary prefs now, we have to do
                // something different. See bug 1666836.
                if (
                  prefBranch.getPrefType(preference) == prefBranch.PREF_INT ||
                  ![0, 1].includes(param[preference].Value)
                ) {
                  prefBranch.setIntPref(preference, param[preference].Value);
                } else {
                  prefBranch.setBoolPref(preference, !!param[preference].Value);
                }
                break;

              case "string":
                prefBranch.setStringPref(preference, param[preference].Value);
                break;
            }
          } catch (e) {
            log.error(
              `Unable to set preference ${preference}. Probable type mismatch.`
            );
          }

          if (param[preference].Status == "locked") {
            Services.prefs.lockPref(preference);
          }
        }
      }
    },
  },

  PrimaryPassword: {
    onAllWindowsRestored(manager, param) {
      if (param) {
        manager.disallowFeature("removeMasterPassword");
      } else {
        manager.disallowFeature("createMasterPassword");
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
        ProxyPolicies.configureProxySettings(
          param,
          PoliciesUtils.setDefaultPref
        );
      }
    },
  },

  RequestedLocales: {
    onBeforeAddons(manager, param) {
      let requestedLocales;
      if (Array.isArray(param)) {
        requestedLocales = param;
      } else if (param) {
        requestedLocales = param.split(",");
      } else {
        requestedLocales = [];
      }
      runOncePerModification(
        "requestedLocales",
        JSON.stringify(requestedLocales),
        () => {
          Services.locale.requestedLocales = requestedLocales;
        }
      );
    },
  },

  SanitizeOnShutdown: {
    onBeforeUIStartup(manager, param) {
      if (typeof param === "boolean") {
        setAndLockPref("privacy.sanitize.sanitizeOnShutdown", param);
        setAndLockPref("privacy.clearOnShutdown.cache", param);
        setAndLockPref("privacy.clearOnShutdown.cookies", param);
        setAndLockPref("privacy.clearOnShutdown.downloads", param);
        setAndLockPref("privacy.clearOnShutdown.formdata", param);
        setAndLockPref("privacy.clearOnShutdown.history", param);
        setAndLockPref("privacy.clearOnShutdown.sessions", param);
        setAndLockPref("privacy.clearOnShutdown.siteSettings", param);
        setAndLockPref("privacy.clearOnShutdown.offlineApps", param);
      } else {
        let locked = true;
        // Needed to preserve original behavior in perpetuity.
        let lockDefaultPrefs = true;
        if ("Locked" in param) {
          locked = param.Locked;
          lockDefaultPrefs = false;
        }
        PoliciesUtils.setDefaultPref(
          "privacy.sanitize.sanitizeOnShutdown",
          true,
          locked
        );
        if ("Cache" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.cache",
            param.Cache,
            locked
          );
        } else {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.cache",
            false,
            lockDefaultPrefs
          );
        }
        if ("Cookies" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.cookies",
            param.Cookies,
            locked
          );
        } else {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.cookies",
            false,
            lockDefaultPrefs
          );
        }
        if ("Downloads" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.downloads",
            param.Downloads,
            locked
          );
        } else {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.downloads",
            false,
            lockDefaultPrefs
          );
        }
        if ("FormData" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.formdata",
            param.FormData,
            locked
          );
        } else {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.formdata",
            false,
            lockDefaultPrefs
          );
        }
        if ("History" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.history",
            param.History,
            locked
          );
        } else {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.history",
            false,
            lockDefaultPrefs
          );
        }
        if ("Sessions" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.sessions",
            param.Sessions,
            locked
          );
        } else {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.sessions",
            false,
            lockDefaultPrefs
          );
        }
        if ("SiteSettings" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.siteSettings",
            param.SiteSettings,
            locked
          );
        }
        if ("OfflineApps" in param) {
          PoliciesUtils.setDefaultPref(
            "privacy.clearOnShutdown.offlineApps",
            param.OfflineApps,
            locked
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
          // Rerun if any engine info has changed.
          let engineInfoHash = md5Hash(JSON.stringify(param.Add));
          await runOncePerModification(
            "addSearchEngines",
            engineInfoHash,
            async function() {
              for (let newEngine of param.Add) {
                let manifest = {
                  description: newEngine.Description,
                  iconURL: newEngine.IconURL ? newEngine.IconURL.href : null,
                  chrome_settings_overrides: {
                    search_provider: {
                      name: newEngine.Name,
                      // If the encoding is not specified or is falsy, the
                      // search service will fall back to the default encoding.
                      encoding: newEngine.Encoding,
                      search_url: encodeURI(newEngine.URLTemplate),
                      keyword: newEngine.Alias,
                      search_url_post_params:
                        newEngine.Method == "POST"
                          ? newEngine.PostData
                          : undefined,
                      suggest_url: newEngine.SuggestURLTemplate,
                    },
                  },
                };
                let engine = Services.search.getEngineByName(newEngine.Name);
                if (engine) {
                  try {
                    await Services.search.updatePolicyEngine(manifest);
                  } catch (ex) {
                    log.error("Unable to update the search engine", ex);
                  }
                } else {
                  try {
                    await Services.search.addPolicyEngine(manifest);
                  } catch (ex) {
                    log.error("Unable to add search engine", ex);
                  }
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
        if (param.DefaultPrivate) {
          await runOncePerModification(
            "setDefaultPrivateSearchEngine",
            param.DefaultPrivate,
            async () => {
              let defaultPrivateEngine;
              try {
                defaultPrivateEngine = Services.search.getEngineByName(
                  param.DefaultPrivate
                );
                if (!defaultPrivateEngine) {
                  throw new Error("No engine by that name could be found");
                }
              } catch (ex) {
                log.error(
                  `Search engine lookup failed when attempting to set ` +
                    `the default private engine. Requested engine was ` +
                    `"${param.DefaultPrivate}".`,
                  ex
                );
              }
              if (defaultPrivateEngine) {
                try {
                  await Services.search.setDefaultPrivate(defaultPrivateEngine);
                } catch (ex) {
                  log.error(
                    "Unable to set the default private search engine",
                    ex
                  );
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

  ShowHomeButton: {
    onBeforeAddons(manager, param) {
      if (param) {
        manager.disallowFeature("removeHomeButtonByDefault");
      }
    },
    onAllWindowsRestored(manager, param) {
      if (param) {
        let homeButtonPlacement = CustomizableUI.getPlacementOfWidget(
          "home-button"
        );
        if (!homeButtonPlacement) {
          let placement = CustomizableUI.getPlacementOfWidget("forward-button");
          CustomizableUI.addWidgetToArea(
            "home-button",
            CustomizableUI.AREA_NAVBAR,
            placement.position + 2
          );
        }
      } else {
        CustomizableUI.removeWidgetFromArea("home-button");
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

  UserMessaging: {
    onBeforeAddons(manager, param) {
      let locked = false;
      if ("Locked" in param) {
        locked = param.Locked;
      }
      if ("WhatsNew" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.messaging-system.whatsNewPanel.enabled",
          param.WhatsNew,
          locked
        );
      }
      if ("ExtensionRecommendations" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
          param.ExtensionRecommendations,
          locked
        );
      }
      if ("FeatureRecommendations" in param) {
        PoliciesUtils.setDefaultPref(
          "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
          param.FeatureRecommendations,
          locked
        );
      }
      if ("UrlbarInterventions" in param && !param.UrlbarInterventions) {
        manager.disallowFeature("urlbarinterventions");
      }
      if ("SkipOnboarding") {
        PoliciesUtils.setDefaultPref(
          "browser.aboutwelcome.enabled",
          !param.SkipOnboarding,
          locked
        );
      }
    },
  },

  WebsiteFilter: {
    onBeforeUIStartup(manager, param) {
      WebsiteFilter.init(param.Block || [], param.Exceptions || []);
    },
  },

  WindowsSSO: {
    onBeforeAddons(manager, param) {
      setAndLockPref("network.http.windows-sso.enabled", param);
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
  PoliciesUtils.setDefaultPref(prefName, prefValue, true);
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

var PoliciesUtils = {
  setDefaultPref(prefName, prefValue, locked = false) {
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

        // This is ugly, but necessary. On Windows GPO and macOS
        // configs, booleans are converted to 0/1. In the previous
        // Preferences implementation, the schema took care of
        // automatically converting these values to booleans.
        // Since we allow arbitrary prefs now, we have to do
        // something different. See bug 1666836.
        if (
          defaults.getPrefType(prefName) == defaults.PREF_INT ||
          ![0, 1].includes(prefValue)
        ) {
          defaults.setIntPref(prefName, prefValue);
        } else {
          defaults.setBoolPref(prefName, !!prefValue);
        }
        break;

      case "string":
        defaults.setStringPref(prefName, prefValue);
        break;
    }

    if (locked) {
      Services.prefs.lockPref(prefName);
    }
  },
};

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
      PoliciesUtils.setDefaultPref(prefName, 2, policyParam.Locked);
    } else {
      PoliciesUtils.setDefaultPref(prefName, 0, policyParam.Locked);
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
function installAddonFromURL(url, extensionID, addon) {
  if (
    addon &&
    addon.sourceURI &&
    addon.sourceURI.spec == url &&
    !addon.sourceURI.schemeIs("file")
  ) {
    // It's the same addon, don't reinstall.
    return;
  }
  AddonManager.getInstallForURL(url, {
    telemetryInfo: { source: "enterprise-policy" },
  }).then(install => {
    if (install.addon && install.addon.appDisabled) {
      log.error(`Incompatible add-on - ${install.addon.id}`);
      install.cancel();
      return;
    }
    let listener = {
      /* eslint-disable-next-line no-shadow */
      onDownloadEnded: install => {
        // Install failed, error will be reported elsewhere.
        if (!install.addon) {
          return;
        }
        if (extensionID && install.addon.id != extensionID) {
          log.error(
            `Add-on downloaded from ${url} had unexpected id (got ${install.addon.id} expected ${extensionID})`
          );
          install.removeListener(listener);
          install.cancel();
        }
        if (install.addon.appDisabled) {
          log.error(`Incompatible add-on - ${url}`);
          install.removeListener(listener);
          install.cancel();
        }
        if (
          addon &&
          Services.vc.compare(addon.version, install.addon.version) == 0
        ) {
          log.debug("Installation cancelled because versions are the same");
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
      /* eslint-disable-next-line no-shadow */
      onInstallEnded: (install, addon) => {
        if (addon.type == "theme") {
          addon.enable();
        }
        install.removeListener(listener);
        log.debug(`Installation succeeded - ${url}`);
      },
    };
    // If it's a local file install, onDownloadEnded is never called.
    // So we call it manually, to handle some error cases.
    if (url.startsWith("file:")) {
      listener.onDownloadEnded(install);
      if (install.state == AddonManager.STATE_CANCELLED) {
        return;
      }
    }
    install.addListener(listener);
    install.install();
  });
}

let gBlockedAboutPages = [];

function clearBlockedAboutPages() {
  gBlockedAboutPages = [];
}

function blockAboutPage(manager, feature, neededOnContentProcess = false) {
  addChromeURLBlocker();
  gBlockedAboutPages.push(feature);

  try {
    let aboutModule = Cc[ABOUT_CONTRACT + feature.split(":")[1]].getService(
      Ci.nsIAboutModule
    );
    let chromeURL = aboutModule.getChromeURI(Services.io.newURI(feature)).spec;
    gBlockedAboutPages.push(chromeURL);
  } catch (e) {
    // Some about pages don't have chrome URLS (compat)
  }
}

let ChromeURLBlockPolicy = {
  shouldLoad(contentLocation, loadInfo, mimeTypeGuess) {
    let contentType = loadInfo.externalContentPolicyType;
    if (
      (contentLocation.scheme != "chrome" &&
        contentLocation.scheme != "about") ||
      (contentType != Ci.nsIContentPolicy.TYPE_DOCUMENT &&
        contentType != Ci.nsIContentPolicy.TYPE_SUBDOCUMENT)
    ) {
      return Ci.nsIContentPolicy.ACCEPT;
    }
    if (
      gBlockedAboutPages.some(function(aboutPage) {
        return contentLocation.spec.startsWith(aboutPage);
      })
    ) {
      return Ci.nsIContentPolicy.REJECT_POLICY;
    }
    return Ci.nsIContentPolicy.ACCEPT;
  },
  shouldProcess(contentLocation, loadInfo, mimeTypeGuess) {
    return Ci.nsIContentPolicy.ACCEPT;
  },
  classDescription: "Policy Engine Content Policy",
  contractID: "@mozilla-org/policy-engine-content-policy-service;1",
  classID: Components.ID("{ba7b9118-cabc-4845-8b26-4215d2a59ed7}"),
  QueryInterface: ChromeUtils.generateQI(["nsIContentPolicy"]),
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  },
};

function addChromeURLBlocker() {
  if (Cc[ChromeURLBlockPolicy.contractID]) {
    return;
  }

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
    .replace(/(.*)-----BEGIN CERTIFICATE-----/, "")
    .replace(/-----END CERTIFICATE-----(.*)/, "")
    .replace(/[\r\n]/g, "");
}

function processMIMEInfo(mimeInfo, realMIMEInfo) {
  if ("handlers" in mimeInfo) {
    let firstHandler = true;
    for (let handler of mimeInfo.handlers) {
      // handler can be null which means they don't
      // want a preferred handler.
      if (handler) {
        let handlerApp;
        if ("path" in handler) {
          try {
            let file = new FileUtils.File(handler.path);
            handlerApp = Cc[
              "@mozilla.org/uriloader/local-handler-app;1"
            ].createInstance(Ci.nsILocalHandlerApp);
            handlerApp.executable = file;
          } catch (ex) {
            log.error(`Unable to create handler executable (${handler.path})`);
            continue;
          }
        } else if ("uriTemplate" in handler) {
          let templateURL = new URL(handler.uriTemplate);
          if (templateURL.protocol != "https:") {
            log.error(`Web handler must be https (${handler.uriTemplate})`);
            continue;
          }
          if (
            !templateURL.pathname.includes("%s") &&
            !templateURL.search.includes("%s")
          ) {
            log.error(`Web handler must contain %s (${handler.uriTemplate})`);
            continue;
          }
          handlerApp = Cc[
            "@mozilla.org/uriloader/web-handler-app;1"
          ].createInstance(Ci.nsIWebHandlerApp);
          handlerApp.uriTemplate = handler.uriTemplate;
        } else {
          log.error("Invalid handler");
          continue;
        }
        if ("name" in handler) {
          handlerApp.name = handler.name;
        }
        realMIMEInfo.possibleApplicationHandlers.appendElement(handlerApp);
        if (firstHandler) {
          realMIMEInfo.preferredApplicationHandler = handlerApp;
        }
      }
      firstHandler = false;
    }
  }
  if ("action" in mimeInfo) {
    let action = realMIMEInfo[mimeInfo.action];
    if (
      action == realMIMEInfo.useHelperApp &&
      !realMIMEInfo.possibleApplicationHandlers.length
    ) {
      log.error("useHelperApp requires a handler");
      return;
    }
    realMIMEInfo.preferredAction = action;
  }
  if ("ask" in mimeInfo) {
    realMIMEInfo.alwaysAskBeforeHandling = mimeInfo.ask;
  }
  gHandlerService.store(realMIMEInfo);
}

// Copied from PlacesUIUtils.jsm

// Keep a hasher for repeated hashings
let gCryptoHash = null;

/**
 * Run some text through md5 and return the base64 result.
 * @param {string} data The string to hash.
 * @returns {string} md5 hash of the input string.
 */
function md5Hash(data) {
  // Lazily create a reusable hasher
  if (gCryptoHash === null) {
    gCryptoHash = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
  }

  gCryptoHash.init(gCryptoHash.MD5);

  // Convert the data to a byte array for hashing
  gCryptoHash.update(
    data.split("").map(c => c.charCodeAt(0)),
    data.length
  );
  // Request the has result as ASCII base64
  return gCryptoHash.finish(true);
}
