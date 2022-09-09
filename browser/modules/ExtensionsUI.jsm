/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionsUI"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AMTelemetry: "resource://gre/modules/AddonManager.jsm",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.jsm",
  OriginControls: "resource://gre/modules/ExtensionPermissions.jsm",
});

const DEFAULT_EXTENSION_ICON =
  "chrome://mozapps/skin/extensions/extensionGeneric.svg";

const BROWSER_PROPERTIES = "chrome://browser/locale/browser.properties";
const BRAND_PROPERTIES = "chrome://branding/locale/brand.properties";

const HTML_NS = "http://www.w3.org/1999/xhtml";

function getTabBrowser(browser) {
  while (browser.ownerGlobal.docShell.itemType !== Ci.nsIDocShell.typeChrome) {
    browser = browser.ownerGlobal.docShell.chromeEventHandler;
  }
  let window = browser.ownerGlobal;
  let viewType = browser.getAttribute("webextension-view-type");
  if (viewType == "sidebar") {
    window = window.browsingContext.topChromeWindow;
  }
  if (viewType == "popup" || viewType == "sidebar") {
    browser = window.gBrowser.selectedBrowser;
  }
  return { browser, window };
}

var ExtensionsUI = {
  sideloaded: new Set(),
  updates: new Set(),
  sideloadListener: null,
  histogram: null,

  pendingNotifications: new WeakMap(),

  async init() {
    this.histogram = Services.telemetry.getHistogramById(
      "EXTENSION_INSTALL_PROMPT_RESULT"
    );

    Services.obs.addObserver(this, "webextension-permission-prompt");
    Services.obs.addObserver(this, "webextension-update-permissions");
    Services.obs.addObserver(this, "webextension-install-notify");
    Services.obs.addObserver(this, "webextension-optional-permission-prompt");
    Services.obs.addObserver(this, "webextension-defaultsearch-prompt");

    await Services.wm.getMostRecentWindow("navigator:browser")
      .delayedStartupPromise;

    this._checkForSideloaded();
  },

  async _checkForSideloaded() {
    let sideloaded = await lazy.AddonManagerPrivate.getNewSideloads();

    if (!sideloaded.length) {
      // No new side-loads. We're done.
      return;
    }

    // The ordering shouldn't matter, but tests depend on notifications
    // happening in a specific order.
    sideloaded.sort((a, b) => a.id.localeCompare(b.id));

    if (!this.sideloadListener) {
      this.sideloadListener = {
        onEnabled: addon => {
          if (!this.sideloaded.has(addon)) {
            return;
          }

          this.sideloaded.delete(addon);
          this._updateNotifications();

          if (this.sideloaded.size == 0) {
            lazy.AddonManager.removeAddonListener(this.sideloadListener);
            this.sideloadListener = null;
          }
        },
      };
      lazy.AddonManager.addAddonListener(this.sideloadListener);
    }

    for (let addon of sideloaded) {
      this.sideloaded.add(addon);
    }
    this._updateNotifications();
  },

  _updateNotifications() {
    if (this.sideloaded.size + this.updates.size == 0) {
      lazy.AppMenuNotifications.removeNotification("addon-alert");
    } else {
      lazy.AppMenuNotifications.showBadgeOnlyNotification("addon-alert");
    }
    this.emit("change");
  },

  showAddonsManager(tabbrowser, strings, icon, histkey) {
    let global = tabbrowser.selectedBrowser.ownerGlobal;
    return global
      .BrowserOpenAddonsMgr("addons://list/extension")
      .then(aomWin => {
        let aomBrowser = aomWin.docShell.chromeEventHandler;
        return this.showPermissionsPrompt(aomBrowser, strings, icon, histkey);
      });
  },

  showSideloaded(tabbrowser, addon) {
    addon.markAsSeen();
    this.sideloaded.delete(addon);
    this._updateNotifications();

    let strings = this._buildStrings({
      addon,
      permissions: addon.userPermissions,
      type: "sideload",
    });

    lazy.AMTelemetry.recordManageEvent(addon, "sideload_prompt", {
      num_strings: strings.msgs.length,
    });

    this.showAddonsManager(tabbrowser, strings, addon.iconURL, "sideload").then(
      async answer => {
        if (answer) {
          await addon.enable();

          this._updateNotifications();

          // The user has just enabled a sideloaded extension, if the permission
          // can be changed for the extension, show the post-install panel to
          // give the user that opportunity.
          if (
            addon.permissions &
            lazy.AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
          ) {
            this.showInstallNotification(tabbrowser.selectedBrowser, addon);
          }
        }
        this.emit("sideload-response");
      }
    );
  },

  showUpdate(browser, info) {
    lazy.AMTelemetry.recordInstallEvent(info.install, {
      step: "permissions_prompt",
      num_strings: info.strings.msgs.length,
    });

    this.showAddonsManager(
      browser,
      info.strings,
      info.addon.iconURL,
      "update"
    ).then(answer => {
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
      let { target, info } = subject.wrappedJSObject;

      let { browser, window } = getTabBrowser(target);

      // Dismiss the progress notification.  Note that this is bad if
      // there are multiple simultaneous installs happening, see
      // bug 1329884 for a longer explanation.
      let progressNotification = window.PopupNotifications.getNotification(
        "addon-progress",
        browser
      );
      if (progressNotification) {
        progressNotification.remove();
      }

      info.unsigned =
        info.addon.signedState <= lazy.AddonManager.SIGNEDSTATE_MISSING;
      if (
        info.unsigned &&
        Cu.isInAutomation &&
        Services.prefs.getBoolPref("extensions.ui.ignoreUnsigned", false)
      ) {
        info.unsigned = false;
      }

      let strings = this._buildStrings(info);

      // If this is an update with no promptable permissions, just apply it
      if (info.type == "update" && !strings.msgs.length) {
        info.resolve();
        return;
      }

      let icon = info.unsigned
        ? "chrome://global/skin/icons/warning.svg"
        : info.icon;

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

      if (info.type == "sideload") {
        lazy.AMTelemetry.recordManageEvent(info.addon, "sideload_prompt", {
          num_strings: strings.msgs.length,
        });
      } else {
        lazy.AMTelemetry.recordInstallEvent(info.install, {
          step: "permissions_prompt",
          num_strings: strings.msgs.length,
        });
      }

      this.showPermissionsPrompt(browser, strings, icon, histkey).then(
        answer => {
          if (answer) {
            info.resolve();
          } else {
            info.reject();
          }
        }
      );
    } else if (topic == "webextension-update-permissions") {
      let info = subject.wrappedJSObject;
      info.type = "update";
      let strings = this._buildStrings(info);

      // If we don't prompt for any new permissions, just apply it
      if (!strings.msgs.length) {
        info.resolve();
        return;
      }

      let update = {
        strings,
        permissions: info.permissions,
        install: info.install,
        addon: info.addon,
        resolve: info.resolve,
        reject: info.reject,
      };

      this.updates.add(update);
      this._updateNotifications();
    } else if (topic == "webextension-install-notify") {
      let { target, addon, callback } = subject.wrappedJSObject;
      this.showInstallNotification(target, addon).then(() => {
        if (callback) {
          callback();
        }
      });
    } else if (topic == "webextension-optional-permission-prompt") {
      let {
        browser,
        name,
        icon,
        permissions,
        resolve,
      } = subject.wrappedJSObject;
      let strings = this._buildStrings({
        type: "optional",
        addon: { name },
        permissions,
      });

      // If we don't have any promptable permissions, just proceed
      if (!strings.msgs.length) {
        resolve(true);
        return;
      }
      resolve(this.showPermissionsPrompt(browser, strings, icon));
    } else if (topic == "webextension-defaultsearch-prompt") {
      let {
        browser,
        name,
        icon,
        respond,
        currentEngine,
        newEngine,
      } = subject.wrappedJSObject;

      let bundle = Services.strings.createBundle(BROWSER_PROPERTIES);

      let strings = {};
      strings.acceptText = bundle.GetStringFromName(
        "webext.defaultSearchYes.label"
      );
      strings.acceptKey = bundle.GetStringFromName(
        "webext.defaultSearchYes.accessKey"
      );
      strings.cancelText = bundle.GetStringFromName(
        "webext.defaultSearchNo.label"
      );
      strings.cancelKey = bundle.GetStringFromName(
        "webext.defaultSearchNo.accessKey"
      );
      strings.addonName = name;
      strings.text = bundle.formatStringFromName(
        "webext.defaultSearch.description",
        ["<>", currentEngine, newEngine]
      );

      this.showDefaultSearchPrompt(browser, strings, icon).then(respond);
    }
  },

  // Create a set of formatted strings for a permission prompt
  _buildStrings(info) {
    let bundle = Services.strings.createBundle(BROWSER_PROPERTIES);
    let brandBundle = Services.strings.createBundle(BRAND_PROPERTIES);
    let appName = brandBundle.GetStringFromName("brandShortName");
    let info2 = Object.assign({ appName }, info);

    let strings = lazy.ExtensionData.formatPermissionStrings(info2, bundle, {
      collapseOrigins: true,
    });
    strings.addonName = info.addon.name;
    strings.learnMore = bundle.GetStringFromName("webextPerms.learnMore2");
    return strings;
  },

  async showPermissionsPrompt(target, strings, icon, histkey) {
    let { browser, window } = getTabBrowser(target);

    // Wait for any pending prompts to complete before showing the next one.
    let pending;
    while ((pending = this.pendingNotifications.get(browser))) {
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
          listIntroEl.hidden = !strings.msgs.length || !strings.listIntro;

          let listInfoEl = doc.getElementById("addon-webext-perm-info");
          listInfoEl.textContent = strings.learnMore;
          listInfoEl.href =
            Services.urlFormatter.formatURLPref("app.support.baseURL") +
            "extension-permissions";
          listInfoEl.hidden = !strings.msgs.length;

          let list = doc.getElementById("addon-webext-perm-list");
          while (list.firstChild) {
            list.firstChild.remove();
          }
          let singleEntryEl = doc.getElementById(
            "addon-webext-perm-single-entry"
          );
          singleEntryEl.textContent = "";
          singleEntryEl.hidden = true;
          list.hidden = true;

          if (strings.msgs.length === 1) {
            singleEntryEl.textContent = strings.msgs[0];
            singleEntryEl.hidden = false;
          } else if (strings.msgs.length) {
            for (let msg of strings.msgs) {
              let item = doc.createElementNS(HTML_NS, "li");
              item.textContent = msg;
              list.appendChild(item);
            }
            list.hidden = false;
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

      let options = {
        hideClose: true,
        popupIconURL: icon || DEFAULT_EXTENSION_ICON,
        popupIconClass: icon ? "" : "addon-warning-icon",
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

      if (browser.ownerGlobal.gUnifiedExtensions.isEnabled) {
        options.popupOptions = {
          position: "bottomright topright",
        };
      }

      window.PopupNotifications.show(
        browser,
        "addon-webext-permissions",
        strings.header,
        browser.ownerGlobal.gUnifiedExtensions.getPopupAnchorID(
          browser,
          window
        ),
        action,
        secondaryActions,
        options
      );
    });

    this.pendingNotifications.set(browser, promise);
    promise.finally(() => this.pendingNotifications.delete(browser));
    return promise;
  },

  showDefaultSearchPrompt(target, strings, icon) {
    return new Promise(resolve => {
      let options = {
        hideClose: true,
        popupIconURL: icon || DEFAULT_EXTENSION_ICON,
        persistent: true,
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

      let { browser, window } = getTabBrowser(target);

      window.PopupNotifications.show(
        browser,
        "addon-webext-defaultsearch",
        strings.text,
        "addons-notification-icon",
        action,
        secondaryActions,
        options
      );
    });
  },

  async showInstallNotification(target, addon) {
    let { window } = getTabBrowser(target);
    let bundle = window.gNavigatorBundle;

    let message = bundle.getFormattedString("addonPostInstall.message3", [
      "<>",
    ]);
    const permissionName = "internal:privateBrowsingAllowed";
    const { permissions } = await lazy.ExtensionPermissions.get(addon.id);
    const hasIncognito = permissions.includes(permissionName);

    return new Promise(resolve => {
      // Show or hide private permission ui based on the pref.
      function setCheckbox(win) {
        let checkbox = win.document.getElementById("addon-incognito-checkbox");
        checkbox.checked = hasIncognito;
        checkbox.hidden = !(
          addon.permissions &
          lazy.AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
        );
      }

      async function actionResolve(win) {
        let checkbox = win.document.getElementById("addon-incognito-checkbox");

        if (checkbox.checked == hasIncognito) {
          resolve();
          return;
        }

        let incognitoPermission = {
          permissions: [permissionName],
          origins: [],
        };

        let value;
        // The checkbox has been changed at this point, otherwise we would
        // have exited early above.
        if (checkbox.checked) {
          await lazy.ExtensionPermissions.add(addon.id, incognitoPermission);
          value = "on";
        } else if (hasIncognito) {
          await lazy.ExtensionPermissions.remove(addon.id, incognitoPermission);
          value = "off";
        }
        if (value !== undefined) {
          lazy.AMTelemetry.recordActionEvent({
            addon,
            object: "doorhanger",
            action: "privateBrowsingAllowed",
            view: "postInstall",
            value,
          });
        }
        // Reload the extension if it is already enabled.  This ensures any change
        // on the private browsing permission is properly handled.
        if (addon.isActive) {
          await addon.reload();
        }

        resolve();
      }

      let action = {
        callback: actionResolve,
      };

      let icon = addon.isWebExtension
        ? lazy.AddonManager.getPreferredIconURL(addon, 32, window) ||
          DEFAULT_EXTENSION_ICON
        : "chrome://browser/skin/addons/addon-install-installed.svg";
      let options = {
        name: addon.name,
        message,
        popupIconURL: icon,
        onRefresh: setCheckbox,
        onDismissed: win => {
          lazy.AppMenuNotifications.removeNotification("addon-installed");
          actionResolve(win);
        },
      };
      lazy.AppMenuNotifications.showNotification(
        "addon-installed",
        action,
        null,
        options
      );
    });
  },

  // Populate extension toolbar popup menu with origin controls.
  originControlsMenu(popup, extensionId) {
    let policy = WebExtensionPolicy.getByID(extensionId);
    if (!policy?.extension.originControls) {
      return;
    }

    popup.ownerGlobal.MozXULElement.insertFTLIfNeeded(
      "preview/originControls.ftl"
    );

    let uri = popup.ownerGlobal.gBrowser.currentURI;
    let state = lazy.OriginControls.getState(policy, uri);

    let doc = popup.ownerDocument;
    let whenClicked, alwaysOn, allDomains;
    let separator = doc.createXULElement("menuseparator");

    let headerItem = doc.createXULElement("menuitem");
    headerItem.setAttribute("disabled", true);

    if (state.noAccess) {
      doc.l10n.setAttributes(headerItem, "origin-controls-no-access");
    } else {
      doc.l10n.setAttributes(headerItem, "origin-controls-options");
    }

    if (state.allDomains) {
      allDomains = doc.createXULElement("menuitem");
      allDomains.setAttribute("type", "radio");
      allDomains.setAttribute("checked", state.hasAccess);
      doc.l10n.setAttributes(allDomains, "origin-controls-option-all-domains");
    }

    if (state.whenClicked) {
      whenClicked = doc.createXULElement("menuitem");
      whenClicked.setAttribute("type", "radio");
      whenClicked.setAttribute("checked", !state.hasAccess);
      doc.l10n.setAttributes(
        whenClicked,
        "origin-controls-option-when-clicked"
      );
      whenClicked.addEventListener("command", () =>
        lazy.OriginControls.setWhenClicked(policy, uri)
      );
    }

    if (state.alwaysOn) {
      alwaysOn = doc.createXULElement("menuitem");
      alwaysOn.setAttribute("type", "radio");
      alwaysOn.setAttribute("checked", state.hasAccess);
      doc.l10n.setAttributes(alwaysOn, "origin-controls-option-always-on", {
        domain: uri.host,
      });
      alwaysOn.addEventListener("command", () =>
        lazy.OriginControls.setAlwaysOn(policy, uri)
      );
    }

    // Insert all before Manage Extension, after any extension's menu items.
    let items = [headerItem, whenClicked, alwaysOn, allDomains, separator];
    let manageItem =
      popup.querySelector(".customize-context-manageExtension") ||
      popup.querySelector(".unified-extensions-context-menu-manage-extension");
    items.forEach(item => item && popup.insertBefore(item, manageItem));

    let cleanup = () => items.forEach(item => item?.remove());
    popup.addEventListener("popuphidden", cleanup, { once: true });
  },
};

EventEmitter.decorate(ExtensionsUI);
