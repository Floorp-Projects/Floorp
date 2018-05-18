/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionsUI"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  Services: "resource://gre/modules/Services.jsm"
});

XPCOMUtils.defineLazyPreferenceGetter(this, "WEBEXT_PERMISSION_PROMPTS",
                                      "extensions.webextPermissionPrompts", false);

const DEFAULT_EXTENSION_ICON = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

const BROWSER_PROPERTIES = "chrome://browser/locale/browser.properties";
const BRAND_PROPERTIES = "chrome://branding/locale/brand.properties";

const HTML_NS = "http://www.w3.org/1999/xhtml";

function getTabBrowser(browser) {
  while (browser.ownerDocument.docShell.itemType !== Ci.nsIDocShell.typeChrome) {
    browser = browser.ownerDocument.docShell.chromeEventHandler;
  }
  return {browser, window: browser.ownerGlobal};
}

var ExtensionsUI = {
  sideloaded: new Set(),
  updates: new Set(),
  sideloadListener: null,
  histogram: null,

  pendingNotifications: new WeakMap(),

  async init() {
    this.histogram = Services.telemetry.getHistogramById("EXTENSION_INSTALL_PROMPT_RESULT");

    Services.obs.addObserver(this, "webextension-permission-prompt");
    Services.obs.addObserver(this, "webextension-update-permissions");
    Services.obs.addObserver(this, "webextension-install-notify");
    Services.obs.addObserver(this, "webextension-optional-permission-prompt");
    Services.obs.addObserver(this, "webextension-defaultsearch-prompt");

    await Services.wm.getMostRecentWindow("navigator:browser").delayedStartupPromise;

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
      let aomBrowser = aomWin.document.docShell.chromeEventHandler;
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
        .then(async answer => {
          if (answer) {
            await addon.enable();
          }
          this.emit("sideload-response");
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

      let {browser, window} = getTabBrowser(target);

      // Dismiss the progress notification.  Note that this is bad if
      // there are multiple simultaneous installs happening, see
      // bug 1329884 for a longer explanation.
      let progressNotification = window.PopupNotifications.getNotification("addon-progress", browser);
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

      this.showPermissionsPrompt(browser, strings, icon, histkey)
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
        resolve(true);
        return;
      }
      resolve(this.showPermissionsPrompt(browser, strings, icon));
    } else if (topic == "webextension-defaultsearch-prompt") {
      let {browser, name, icon, resolve, currentEngine, newEngine} = subject.wrappedJSObject;

      let bundle = Services.strings.createBundle(BROWSER_PROPERTIES);

      let strings = {};
      strings.acceptText = bundle.GetStringFromName("webext.defaultSearchYes.label");
      strings.acceptKey = bundle.GetStringFromName("webext.defaultSearchYes.accessKey");
      strings.cancelText = bundle.GetStringFromName("webext.defaultSearchNo.label");
      strings.cancelKey = bundle.GetStringFromName("webext.defaultSearchNo.accessKey");
      strings.addonName = name;
      strings.text = bundle.formatStringFromName("webext.defaultSearch.description",
                                                 ["<>", currentEngine, newEngine], 3);
      resolve(this.showDefaultSearchPrompt(browser, strings, icon));
    }
  },

  // Create a set of formatted strings for a permission prompt
  _buildStrings(info) {
    let bundle = Services.strings.createBundle(BROWSER_PROPERTIES);
    let brandBundle = Services.strings.createBundle(BRAND_PROPERTIES);
    let appName = brandBundle.GetStringFromName("brandShortName");
    let info2 = Object.assign({appName}, info);

    let strings = ExtensionData.formatPermissionStrings(info2, bundle);
    strings.addonName = info.addon.name;
    return strings;
  },

  async showPermissionsPrompt(target, strings, icon, histkey) {
    let {browser, window} = getTabBrowser(target);

    // Wait for any pending prompts in this window to complete before
    // showing the next one.
    let pending;
    while ((pending = this.pendingNotifications.get(window))) {
      await pending;
    }

    let promise = new Promise(resolve => {
      function eventCallback(topic) {
        let doc = this.browser.ownerDocument;
        if (topic == "showing") {
          let textEl = doc.getElementById("addon-webext-perm-text");
          textEl.textContent = strings.text;
          textEl.hidden = !strings.text;

          let listIntroEl = doc.getElementById("addon-webext-perm-intro");
          listIntroEl.textContent = strings.listIntro;
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
        if (topic == "removed") {
          Services.tm.dispatchToMainThread(() => {
            resolve(false);
          });
        }
        return false;
      }

      let popupOptions = {
        hideClose: true,
        popupIconURL: icon || DEFAULT_EXTENSION_ICON,
        persistent: true,
        eventCallback,
        name: strings.addonName,
        removeOnDismissal: true,
      };

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

      window.PopupNotifications.show(browser, "addon-webext-permissions", strings.header,
                                     "addons-notification-icon", action,
                                     secondaryActions, popupOptions);
    });

    this.pendingNotifications.set(window, promise);
    promise.finally(() => this.pendingNotifications.delete(window));
    return promise;
  },

  showDefaultSearchPrompt(target, strings, icon) {
    return new Promise(resolve => {
      let popupOptions = {
        hideClose: true,
        popupIconURL: icon || DEFAULT_EXTENSION_ICON,
        persistent: false,
        removeOnDismissal: true,
        eventCallback(topic) {
          if (topic == "removed") {
            resolve(false);
          }
        },
        name: strings.addonName,
      };

      let action = {
        label: strings.acceptText,
        accessKey: strings.acceptKey,
        disableHighlight: true,
        callback: () => {
          resolve(true);
        },
      };
      let secondaryActions = [
        {
          label: strings.cancelText,
          accessKey: strings.cancelKey,
          callback: () => {
            resolve(false);
          },
        },
      ];

      let {browser, window} = getTabBrowser(target);
      window.PopupNotifications.show(browser, "addon-webext-defaultsearch", strings.text,
                                     "addons-notification-icon", action,
                                     secondaryActions, popupOptions);
    });
  },

  showInstallNotification(target, addon) {
    let {browser, window} = getTabBrowser(target);
    let popups = window.PopupNotifications;

    let brandBundle = window.document.getElementById("bundle_brand");
    let appName = brandBundle.getString("brandShortName");
    let bundle = window.gNavigatorBundle;

    let message = bundle.getFormattedString("addonPostInstall.message1",
                                            ["<>", appName]);
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
          if (topic == "dismissed") {
            resolve();
          }
        },
        name: addon.name,
      };

      popups.show(browser, "addon-installed", message, "addons-notification-icon",
                  action, null, options);
    });
  },
};

EventEmitter.decorate(ExtensionsUI);
