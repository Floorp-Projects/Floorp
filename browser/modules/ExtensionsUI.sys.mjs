/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AMBrowserExtensionsImport: "resource://gre/modules/AddonManager.sys.mjs",
  AMTelemetry: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.sys.mjs",
  ExtensionData: "resource://gre/modules/Extension.sys.mjs",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.sys.mjs",
  OriginControls: "resource://gre/modules/ExtensionPermissions.sys.mjs",
  QuarantinedDomains: "resource://gre/modules/ExtensionPermissions.sys.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "l10n",
  () =>
    new Localization(["browser/extensionsUI.ftl", "branding/brand.ftl"], true)
);

const DEFAULT_EXTENSION_ICON =
  "chrome://mozapps/skin/extensions/extensionGeneric.svg";

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

export var ExtensionsUI = {
  sideloaded: new Set(),
  updates: new Set(),
  sideloadListener: null,

  pendingNotifications: new WeakMap(),

  async init() {
    Services.obs.addObserver(this, "webextension-permission-prompt");
    Services.obs.addObserver(this, "webextension-update-permissions");
    Services.obs.addObserver(this, "webextension-install-notify");
    Services.obs.addObserver(this, "webextension-optional-permission-prompt");
    Services.obs.addObserver(this, "webextension-defaultsearch-prompt");
    Services.obs.addObserver(this, "webextension-imported-addons-cancelled");
    Services.obs.addObserver(this, "webextension-imported-addons-complete");
    Services.obs.addObserver(this, "webextension-imported-addons-pending");

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
    const { sideloaded, updates } = this;
    const { importedAddonIDs } = lazy.AMBrowserExtensionsImport;

    if (importedAddonIDs.length + sideloaded.size + updates.size == 0) {
      lazy.AppMenuNotifications.removeNotification("addon-alert");
    } else {
      lazy.AppMenuNotifications.showBadgeOnlyNotification("addon-alert");
    }
    this.emit("change");
  },

  showAddonsManager(tabbrowser, strings, icon) {
    let global = tabbrowser.selectedBrowser.ownerGlobal;
    return global
      .BrowserOpenAddonsMgr("addons://list/extension")
      .then(aomWin => {
        let aomBrowser = aomWin.docShell.chromeEventHandler;
        return this.showPermissionsPrompt(aomBrowser, strings, icon);
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

    this.showAddonsManager(tabbrowser, strings, addon.iconURL).then(
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

    this.showAddonsManager(browser, info.strings, info.addon.iconURL).then(
      answer => {
        if (answer) {
          info.resolve();
        } else {
          info.reject();
        }
        // At the moment, this prompt will re-appear next time we do an update
        // check.  See bug 1332360 for proposal to avoid this.
        this.updates.delete(info);
        this._updateNotifications();
      }
    );
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

      this.showPermissionsPrompt(browser, strings, icon).then(answer => {
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
      let { browser, name, icon, permissions, resolve } =
        subject.wrappedJSObject;
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
      let { browser, name, icon, respond, currentEngine, newEngine } =
        subject.wrappedJSObject;

      const [searchDesc, searchYes, searchNo] = lazy.l10n.formatMessagesSync([
        {
          id: "webext-default-search-description",
          args: { addonName: "<>", currentEngine, newEngine },
        },
        "webext-default-search-yes",
        "webext-default-search-no",
      ]);

      const strings = { addonName: name, text: searchDesc.value };
      for (let attr of searchYes.attributes) {
        if (attr.name === "label") {
          strings.acceptText = attr.value;
        } else if (attr.name === "accesskey") {
          strings.acceptKey = attr.value;
        }
      }
      for (let attr of searchNo.attributes) {
        if (attr.name === "label") {
          strings.cancelText = attr.value;
        } else if (attr.name === "accesskey") {
          strings.cancelKey = attr.value;
        }
      }

      this.showDefaultSearchPrompt(browser, strings, icon).then(respond);
    } else if (
      [
        "webextension-imported-addons-cancelled",
        "webextension-imported-addons-complete",
        "webextension-imported-addons-pending",
      ].includes(topic)
    ) {
      this._updateNotifications();
    }
  },

  // Create a set of formatted strings for a permission prompt
  _buildStrings(info) {
    const strings = lazy.ExtensionData.formatPermissionStrings(info, {
      collapseOrigins: true,
    });
    strings.addonName = info.addon.name;
    return strings;
  },

  async showPermissionsPrompt(target, strings, icon) {
    let { browser, window } = getTabBrowser(target);

    await window.ensureCustomElements("moz-support-link");

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

          // By default, multiline strings don't get formatted properly. These
          // are presently only used in site permission add-ons, so we treat it
          // as a special case to avoid unintended effects on other things.
          let isMultiline = strings.text.includes("\n\n");
          textEl.classList.toggle(
            "addon-webext-perm-text-multiline",
            isMultiline
          );

          let listIntroEl = doc.getElementById("addon-webext-perm-intro");
          listIntroEl.textContent = strings.listIntro;
          listIntroEl.hidden = !strings.msgs.length || !strings.listIntro;

          let listInfoEl = doc.getElementById("addon-webext-perm-info");
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
        removeOnDismissal: true,
        popupOptions: {
          position: "bottomright topright",
        },
      };
      // The prompt/notification machinery has a special affordance wherein
      // certain subsets of the header string can be designated "names", and
      // referenced symbolically as "<>" and "{}" to receive special formatting.
      // That code assumes that the existence of |name| and |secondName| in the
      // options object imply the presence of "<>" and "{}" (respectively) in
      // in the string.
      //
      // At present, WebExtensions use this affordance while SitePermission
      // add-ons don't, so we need to conditionally set the |name| field.
      //
      // NB: This could potentially be cleaned up, see bug 1799710.
      if (strings.header.includes("<>")) {
        options.name = strings.addonName;
      }

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

    const message = await lazy.l10n.formatValue("addon-post-install-message", {
      addonName: "<>",
    });
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

        // The checkbox has been changed at this point, otherwise we would
        // have exited early above.
        if (checkbox.checked) {
          await lazy.ExtensionPermissions.add(addon.id, incognitoPermission);
        } else if (hasIncognito) {
          await lazy.ExtensionPermissions.remove(addon.id, incognitoPermission);
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

  async showQuarantineConfirmation(browser, policy) {
    let [title, line1, line2, allow, deny] = await lazy.l10n.formatMessages([
      {
        id: "webext-quarantine-confirmation-title",
        args: { addonName: "<>" },
      },
      "webext-quarantine-confirmation-line-1",
      "webext-quarantine-confirmation-line-2",
      "webext-quarantine-confirmation-allow",
      "webext-quarantine-confirmation-deny",
    ]);

    let attr = (msg, name) => msg.attributes.find(a => a.name === name)?.value;

    let strings = {
      addonName: policy.name,
      header: title.value,
      text: line1.value + "\n\n" + line2.value,
      msgs: [],
      acceptText: attr(allow, "label"),
      acceptKey: attr(allow, "accesskey"),
      cancelText: attr(deny, "label"),
      cancelKey: attr(deny, "accesskey"),
    };

    let icon = policy.extension?.getPreferredIcon(32);

    if (await ExtensionsUI.showPermissionsPrompt(browser, strings, icon)) {
      lazy.QuarantinedDomains.setUserAllowedAddonIdPref(policy.id, true);
    }
  },

  // Populate extension toolbar popup menu with origin controls.
  originControlsMenu(popup, extensionId) {
    let policy = WebExtensionPolicy.getByID(extensionId);

    let win = popup.ownerGlobal;
    let doc = popup.ownerDocument;
    let tab = win.gBrowser.selectedTab;
    let uri = tab.linkedBrowser?.currentURI;
    let state = lazy.OriginControls.getState(policy, tab);

    let headerItem = doc.createXULElement("menuitem");
    headerItem.setAttribute("disabled", true);
    let items = [headerItem];

    // MV2 normally don't have controls, but we show the quarantined state.
    if (!policy?.extension.originControls && !state.quarantined) {
      return;
    }

    if (state.noAccess) {
      doc.l10n.setAttributes(headerItem, "origin-controls-no-access");
    } else {
      doc.l10n.setAttributes(headerItem, "origin-controls-options");
    }

    if (state.quarantined) {
      doc.l10n.setAttributes(headerItem, "origin-controls-quarantined-status");

      let allowQuarantined = doc.createXULElement("menuitem");
      doc.l10n.setAttributes(
        allowQuarantined,
        "origin-controls-quarantined-allow"
      );
      allowQuarantined.addEventListener("command", () => {
        this.showQuarantineConfirmation(tab.linkedBrowser, policy);
      });
      items.push(allowQuarantined);
    }

    if (state.allDomains) {
      let allDomains = doc.createXULElement("menuitem");
      allDomains.setAttribute("type", "radio");
      allDomains.setAttribute("checked", state.hasAccess);
      doc.l10n.setAttributes(allDomains, "origin-controls-option-all-domains");
      items.push(allDomains);
    }

    if (state.whenClicked) {
      let whenClicked = doc.createXULElement("menuitem");
      whenClicked.setAttribute("type", "radio");
      whenClicked.setAttribute("checked", !state.hasAccess);
      doc.l10n.setAttributes(
        whenClicked,
        "origin-controls-option-when-clicked"
      );
      whenClicked.addEventListener("command", async () => {
        await lazy.OriginControls.setWhenClicked(policy, uri);
        win.gUnifiedExtensions.updateAttention();
      });
      items.push(whenClicked);
    }

    if (state.alwaysOn) {
      let alwaysOn = doc.createXULElement("menuitem");
      alwaysOn.setAttribute("type", "radio");
      alwaysOn.setAttribute("checked", state.hasAccess);
      doc.l10n.setAttributes(alwaysOn, "origin-controls-option-always-on", {
        domain: uri.host,
      });
      alwaysOn.addEventListener("command", async () => {
        await lazy.OriginControls.setAlwaysOn(policy, uri);
        win.gUnifiedExtensions.updateAttention();
      });
      items.push(alwaysOn);
    }

    items.push(doc.createXULElement("menuseparator"));

    // Insert all items before Pin to toolbar OR Manage Extension, but after
    // any extension's menu items.
    let manageItem =
      popup.querySelector(".customize-context-manageExtension") ||
      popup.querySelector(".unified-extensions-context-menu-pin-to-toolbar");
    items.forEach(item => item && popup.insertBefore(item, manageItem));

    let cleanup = e => {
      if (e.target === popup) {
        items.forEach(item => item?.remove());
        popup.removeEventListener("popuphidden", cleanup);
      }
    };
    popup.addEventListener("popuphidden", cleanup);
  },
};

EventEmitter.decorate(ExtensionsUI);
