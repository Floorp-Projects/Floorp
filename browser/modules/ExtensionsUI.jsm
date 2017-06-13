/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["ExtensionsUI"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppMenuNotifications",
                                  "resource://gre/modules/AppMenuNotifications.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "WEBEXT_PERMISSION_PROMPTS",
                                      "extensions.webextPermissionPrompts", false);

const DEFAULT_EXTENSION_ICON = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

const BROWSER_PROPERTIES = "chrome://browser/locale/browser.properties";
const BRAND_PROPERTIES = "chrome://branding/locale/brand.properties";

const HTML_NS = "http://www.w3.org/1999/xhtml";

this.ExtensionsUI = {
  sideloaded: new Set(),
  updates: new Set(),
  sideloadListener: null,
  histogram: null,

  async init() {
    this.histogram = Services.telemetry.getHistogramById("EXTENSION_INSTALL_PROMPT_RESULT");

    Services.obs.addObserver(this, "webextension-permission-prompt");
    Services.obs.addObserver(this, "webextension-update-permissions");
    Services.obs.addObserver(this, "webextension-install-notify");
    Services.obs.addObserver(this, "webextension-optional-permission-prompt");

    await RecentWindow.getMostRecentBrowserWindow().delayedStartupPromise;

    this._checkForSideloaded();
  },

  async _checkForSideloaded() {
    let sideloaded = await AddonManagerPrivate.getNewSideloads();

    if (!sideloaded.length) {
      // No new side-loads. We're done.
      return;
    }

    // The ordering shouldn't matter, but tests depend on notifications
    // happening in a specific order.
    sideloaded.sort((a, b) => a.id.localeCompare(b.id));

    if (WEBEXT_PERMISSION_PROMPTS) {
      if (!this.sideloadListener) {
        this.sideloadListener = {
          onEnabled: addon => {
            if (!this.sideloaded.has(addon)) {
              return;
            }

            this.sideloaded.delete(addon);
              this._updateNotifications();

            if (this.sideloaded.size == 0) {
              AddonManager.removeAddonListener(this.sideloadListener);
              this.sideloadListener = null;
            }
          },
        };
        AddonManager.addAddonListener(this.sideloadListener);
      }

      for (let addon of sideloaded) {
        this.sideloaded.add(addon);
      }
        this._updateNotifications();
    } else {
      // This and all the accompanying about:newaddon code can eventually
      // be removed.  See bug 1331521.
      let win = RecentWindow.getMostRecentBrowserWindow();
      for (let addon of sideloaded) {
        win.openUILinkIn(`about:newaddon?id=${addon.id}`, "tab");
      }
    }
  },

  _updateNotifications() {
    if (this.sideloaded.size + this.updates.size == 0) {
      AppMenuNotifications.removeNotification("addon-alert");
    } else {
      AppMenuNotifications.showBadgeOnlyNotification("addon-alert");
    }
    this.emit("change");
  },

  showAddonsManager(browser, strings, icon, histkey) {
    let global = browser.selectedBrowser.ownerGlobal;
    return global.BrowserOpenAddonsMgr("addons://list/extension").then(aomWin => {
      let aomBrowser = aomWin.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDocShell)
                             .chromeEventHandler;
      return this.showPermissionsPrompt(aomBrowser, strings, icon, histkey);
    });
  },

  showSideloaded(browser, addon) {
    addon.markAsSeen();
    this.sideloaded.delete(addon);
    this._updateNotifications();

    let strings = this._buildStrings({
      addon,
      permissions: addon.userPermissions,
      type: "sideload",
    });
    this.showAddonsManager(browser, strings, addon.iconURL, "sideload")
        .then(answer => {
          addon.userDisabled = !answer;
        });
  },

  showUpdate(browser, info) {
    this.showAddonsManager(browser, info.strings, info.addon.iconURL, "update")
        .then(answer => {
          if (answer) {
            info.resolve();
          } else {
            info.reject();
          }
          // At the moment, this prompt will re-appear next time we do an update
          // check.  See bug 1332360 for proposal to avoid this.
          this.updates.delete(info);
          this._updateNotifications();
        });
  },

  observe(subject, topic, data) {
    if (topic == "webextension-permission-prompt") {
      let {target, info} = subject.wrappedJSObject;

      // Dismiss the progress notification.  Note that this is bad if
      // there are multiple simultaneous installs happening, see
      // bug 1329884 for a longer explanation.
      let progressNotification = target.ownerGlobal.PopupNotifications.getNotification("addon-progress", target);
      if (progressNotification) {
        progressNotification.remove();
      }

      info.unsigned = info.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING;
      if (info.unsigned && Cu.isInAutomation &&
          Services.prefs.getBoolPref("extensions.ui.ignoreUnsigned", false)) {
        info.unsigned = false;
      }

      let strings = this._buildStrings(info);

      // If this is an update with no promptable permissions, just apply it
      if (info.type == "update" && strings.msgs.length == 0) {
        info.resolve();
        return;
      }

      let icon = info.unsigned ? "chrome://browser/skin/warning.svg" : info.icon;

      let histkey;
      if (info.type == "sideload") {
        histkey = "sideload";
      } else if (info.type == "update") {
        histkey = "update";
      } else if (info.source == "AMO") {
        histkey = "installAmo";
      } else if (info.source == "local") {
        histkey = "installLocal";
      } else {
        histkey = "installWeb";
      }

      this.showPermissionsPrompt(target, strings, icon, histkey)
          .then(answer => {
            if (answer) {
              info.resolve();
            } else {
              info.reject();
            }
          });
    } else if (topic == "webextension-update-permissions") {
      let info = subject.wrappedJSObject;
      info.type = "update";
      let strings = this._buildStrings(info);

      // If we don't prompt for any new permissions, just apply it
      if (strings.msgs.length == 0) {
        info.resolve();
        return;
      }

      let update = {
        strings,
        addon: info.addon,
        resolve: info.resolve,
        reject: info.reject,
      };

      this.updates.add(update);
      this._updateNotifications();
    } else if (topic == "webextension-install-notify") {
      let {target, addon, callback} = subject.wrappedJSObject;
      this.showInstallNotification(target, addon).then(() => {
        if (callback) {
          callback();
        }
      });
    } else if (topic == "webextension-optional-permission-prompt") {
      let {browser, name, icon, permissions, resolve} = subject.wrappedJSObject;
      let strings = this._buildStrings({
        type: "optional",
        addon: {name},
        permissions,
      });

      // If we don't have any promptable permissions, just proceed
      if (strings.msgs.length == 0) {
        resolve();
        return;
      }

      resolve(this.showPermissionsPrompt(browser, strings, icon));
    }
  },

  // Escape &, <, and > characters in a string so that it may be
  // injected as part of raw markup.
  _sanitizeName(name) {
    return name.replace(/&/g, "&amp;")
               .replace(/</g, "&lt;")
               .replace(/>/g, "&gt;");
  },

  // Create a set of formatted strings for a permission prompt
  _buildStrings(info) {
    let result = {};

    let bundle = Services.strings.createBundle(BROWSER_PROPERTIES);

    let perms = info.permissions || {origins: [], permissions: []};

    // First classify our host permissions
    let allUrls = false, wildcards = [], sites = [];
    for (let permission of perms.origins) {
      if (permission == "<all_urls>") {
        allUrls = true;
        break;
      }
      let match = /^[htps*]+:\/\/([^/]+)\//.exec(permission);
      if (!match) {
        Cu.reportError(`Unparseable host permission ${permission}`);
        continue;
      }
      if (match[1] == "*") {
        allUrls = true;
      } else if (match[1].startsWith("*.")) {
        wildcards.push(match[1].slice(2));
      } else {
        sites.push(match[1]);
      }
    }

    // Format the host permissions.  If we have a wildcard for all urls,
    // a single string will suffice.  Otherwise, show domain wildcards
    // first, then individual host permissions.
    result.msgs = [];
    if (allUrls) {
      result.msgs.push(bundle.GetStringFromName("webextPerms.hostDescription.allUrls"));
    } else {
      // Formats a list of host permissions.  If we have 4 or fewer, display
      // them all, otherwise display the first 3 followed by an item that
      // says "...plus N others"
      function format(list, itemKey, moreKey) {
        function formatItems(items) {
          result.msgs.push(...items.map(item => bundle.formatStringFromName(itemKey, [item], 1)));
        }
        if (list.length < 5) {
          formatItems(list);
        } else {
          formatItems(list.slice(0, 3));

          let remaining = list.length - 3;
          result.msgs.push(PluralForm.get(remaining, bundle.GetStringFromName(moreKey))
                                     .replace("#1", remaining));
        }
      }

      format(wildcards, "webextPerms.hostDescription.wildcard",
             "webextPerms.hostDescription.tooManyWildcards");
      format(sites, "webextPerms.hostDescription.oneSite",
             "webextPerms.hostDescription.tooManySites");
    }

    let permissionKey = perm => `webextPerms.description.${perm}`;

    // Next, show the native messaging permission if it is present.
    const NATIVE_MSG_PERM = "nativeMessaging";
    if (perms.permissions.includes(NATIVE_MSG_PERM)) {
      let brandBundle = Services.strings.createBundle(BRAND_PROPERTIES);
      let appName = brandBundle.GetStringFromName("brandShortName");
      result.msgs.push(bundle.formatStringFromName(permissionKey(NATIVE_MSG_PERM), [appName], 1));
    }

    // Finally, show remaining permissions, in any order.
    for (let permission of perms.permissions) {
      // Handled above
      if (permission == "nativeMessaging") {
        continue;
      }
      try {
        result.msgs.push(bundle.GetStringFromName(permissionKey(permission)));
      } catch (err) {
        // We deliberately do not include all permissions in the prompt.
        // So if we don't find one then just skip it.
      }
    }

    // Now figure out all the rest of the notification text.
    let name = this._sanitizeName(info.addon.name);
    let addonName = `<span class="addon-webext-name">${name}</span>`;

    result.header = bundle.formatStringFromName("webextPerms.header", [addonName], 1);
    result.text = info.unsigned ?
                  bundle.GetStringFromName("webextPerms.unsignedWarning") : "";
    result.listIntro = bundle.GetStringFromName("webextPerms.listIntro");

    result.acceptText = bundle.GetStringFromName("webextPerms.add.label");
    result.acceptKey = bundle.GetStringFromName("webextPerms.add.accessKey");
    result.cancelText = bundle.GetStringFromName("webextPerms.cancel.label");
    result.cancelKey = bundle.GetStringFromName("webextPerms.cancel.accessKey");

    if (info.type == "sideload") {
      result.header = bundle.formatStringFromName("webextPerms.sideloadHeader", [addonName], 1);
      let key = result.msgs.length == 0 ?
                "webextPerms.sideloadTextNoPerms" : "webextPerms.sideloadText2";
      result.text = bundle.GetStringFromName(key);
      result.acceptText = bundle.GetStringFromName("webextPerms.sideloadEnable.label");
      result.acceptKey = bundle.GetStringFromName("webextPerms.sideloadEnable.accessKey");
      result.cancelText = bundle.GetStringFromName("webextPerms.sideloadCancel.label");
      result.cancelKey = bundle.GetStringFromName("webextPerms.sideloadCancel.accessKey");
    } else if (info.type == "update") {
      result.header = "";
      result.text = bundle.formatStringFromName("webextPerms.updateText", [addonName], 1);
      result.acceptText = bundle.GetStringFromName("webextPerms.updateAccept.label");
      result.acceptKey = bundle.GetStringFromName("webextPerms.updateAccept.accessKey");
    } else if (info.type == "optional") {
      result.header = bundle.formatStringFromName("webextPerms.optionalPermsHeader", [addonName], 1);
      result.text = "";
      result.listIntro = bundle.GetStringFromName("webextPerms.optionalPermsListIntro");
      result.acceptText = bundle.GetStringFromName("webextPerms.optionalPermsAllow.label");
      result.acceptKey = bundle.GetStringFromName("webextPerms.optionalPermsAllow.accessKey");
      result.cancelText = bundle.GetStringFromName("webextPerms.optionalPermsDeny.label");
    }

    return result;
  },

  showPermissionsPrompt(browser, strings, icon, histkey) {
    function eventCallback(topic) {
      let doc = this.browser.ownerDocument;
      if (topic == "showing") {
        doc.getElementById("addon-webext-perm-header").innerHTML = strings.header;
        let textEl = doc.getElementById("addon-webext-perm-text");
        textEl.innerHTML = strings.text;
        textEl.hidden = !strings.text;

        let listIntroEl = doc.getElementById("addon-webext-perm-intro");
        listIntroEl.value = strings.listIntro;
        listIntroEl.hidden = (strings.msgs.length == 0);

        let list = doc.getElementById("addon-webext-perm-list");
        while (list.firstChild) {
          list.firstChild.remove();
        }

        for (let msg of strings.msgs) {
          let item = doc.createElementNS(HTML_NS, "li");
          item.textContent = msg;
          list.appendChild(item);
        }
      } else if (topic == "swapping") {
        return true;
      }
      return false;
    }

    let popupOptions = {
      hideClose: true,
      popupIconURL: icon || DEFAULT_EXTENSION_ICON,
      persistent: true,
      eventCallback,
    };

    let win = browser.ownerGlobal;
    return new Promise(resolve => {
      let action = {
        label: strings.acceptText,
        accessKey: strings.acceptKey,
        callback: () => {
          if (histkey) {
            this.histogram.add(histkey + "Accepted");
          }
          resolve(true);
        },
      };
      let secondaryActions = [
        {
          label: strings.cancelText,
          accessKey: strings.cancelKey,
          callback: () => {
            if (histkey) {
              this.histogram.add(histkey + "Rejected");
            }
            resolve(false);
          },
        },
      ];

      win.PopupNotifications.show(browser, "addon-webext-permissions", "",
                                  "addons-notification-icon",
                                  action, secondaryActions, popupOptions);
    });
  },

  showInstallNotification(target, addon) {
    let win = target.ownerGlobal;
    let popups = win.PopupNotifications;

    let name = this._sanitizeName(addon.name);
    let addonName = `<span class="addon-webext-name">${name}</span>`;
    let addonIcon = '<image class="addon-addon-icon"/>';
    let toolbarIcon = '<image class="addon-toolbar-icon"/>';

    let brandBundle = win.document.getElementById("bundle_brand");
    let appName = brandBundle.getString("brandShortName");

    let bundle = win.gNavigatorBundle;
    let msg1 = bundle.getFormattedString("addonPostInstall.message1",
                                         [addonName, appName]);
    let msg2 = bundle.getFormattedString("addonPostInstall.messageDetail",
                                         [addonIcon, toolbarIcon]);

    return new Promise(resolve => {
      let action = {
        label: bundle.getString("addonPostInstall.okay.label"),
        accessKey: bundle.getString("addonPostInstall.okay.key"),
        callback: resolve,
      };

      let icon = addon.isWebExtension ?
                 addon.iconURL || DEFAULT_EXTENSION_ICON :
                 "chrome://browser/skin/addons/addon-install-installed.svg";
      let options = {
        hideClose: true,
        timeout: Date.now() + 30000,
        popupIconURL: icon,
        eventCallback(topic) {
          if (topic == "showing") {
            let doc = this.browser.ownerDocument;
            doc.getElementById("addon-installed-notification-header")
               .innerHTML = msg1;
            doc.getElementById("addon-installed-notification-message")
               .innerHTML = msg2;
          } else if (topic == "dismissed") {
            resolve();
          }
        }
      };

      popups.show(target, "addon-installed", "", "addons-notification-icon",
                  action, null, options);
    });
  },
};

EventEmitter.decorate(ExtensionsUI);
