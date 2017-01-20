/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["ExtensionsUI"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "WEBEXT_PERMISSION_PROMPTS",
                                      "extensions.webextPermissionPrompts", false);

const DEFAULT_EXENSION_ICON = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

const HTML_NS = "http://www.w3.org/1999/xhtml";

this.ExtensionsUI = {
  sideloaded: new Set(),
  updates: new Set(),

  init() {
    Services.obs.addObserver(this, "webextension-permission-prompt", false);
    Services.obs.addObserver(this, "webextension-update-permissions", false);

    this._checkForSideloaded();
  },

  _checkForSideloaded() {
    AddonManager.getAllAddons(addons => {
      // Check for any side-loaded addons that the user is allowed
      // to enable.
      let sideloaded = addons.filter(
        addon => addon.seen === false && (addon.permissions & AddonManager.PERM_CAN_ENABLE));

      if (!sideloaded.length) {
        return;
      }

      if (WEBEXT_PERMISSION_PROMPTS) {
        for (let addon of sideloaded) {
          this.sideloaded.add(addon);
        }
        this.emit("change");
      } else {
        // This and all the accompanying about:newaddon code can eventually
        // be removed.  See bug 1331521.
        let win = RecentWindow.getMostRecentBrowserWindow();
        for (let addon of sideloaded) {
          win.openUILinkIn(`about:newaddon?id=${addon.id}`, "tab");
        }
      }
    });
  },

  showAddonsManager(browser, info) {
    let loadPromise = new Promise(resolve => {
      let listener = (subject, topic) => {
        if (subject.location.href == "about:addons") {
          Services.obs.removeObserver(listener, topic);
          resolve(subject);
        }
      };
      Services.obs.addObserver(listener, "EM-loaded", false);
    });
    let tab = browser.addTab("about:addons");
    browser.selectedTab = tab;

    return loadPromise.then(win => {
      win.loadView("addons://list/extension");
      return this.showPermissionsPrompt(browser.selectedBrowser, info);
    });
  },

  showSideloaded(browser, addon) {
    addon.markAsSeen();
    this.sideloaded.delete(addon);
    this.emit("change");

    let info = {
      addon,
      permissions: addon.userPermissions,
      icon: addon.iconURL,
      type: "sideload",
    };
    this.showAddonsManager(browser, info).then(answer => {
      addon.userDisabled = !answer;
    });
  },

  showUpdate(browser, info) {
    info.type = "update";
    this.showAddonsManager(browser, info).then(answer => {
      if (answer) {
        info.resolve();
      } else {
        info.reject();
      }
      // At the moment, this prompt will re-appear next time we do an update
      // check.  See bug 1332360 for proposal to avoid this.
      this.updates.delete(info);
      this.emit("change");
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

      let reply = answer => {
        Services.obs.notifyObservers(subject, "webextension-permission-response",
                                     JSON.stringify(answer));
      };

      let perms = info.addon.userPermissions;
      if (!perms) {
        reply(true);
      } else {
        info.permissions = perms;
        this.showPermissionsPrompt(target, info).then(reply);
      }
    } else if (topic == "webextension-update-permissions") {
      this.updates.add(subject.wrappedJSObject);
      this.emit("change");
    }
  },

  showPermissionsPrompt(target, info) {
    let perms = info.permissions;
    if (!perms) {
      return Promise.resolve();
    }

    let win = target.ownerGlobal;

    let name = info.addon.name;
    if (name.length > 50) {
      name = name.slice(0, 49) + "…";
    }

    // The strings below are placeholders, they will switch over to the
    // bundle.get*String() calls as part of bug 1316996.

    // let bundle = win.gNavigatorBundle;
    // let header = bundle.getFormattedString("webextPerms.header", [name])
    // let listHeader = bundle.getString("webextPerms.listHeader");
    let header = "Add ADDON?".replace("ADDON", name);
    let text = "";
    let listHeader = "It can:";

    // let acceptText = bundle.getString("webextPerms.accept.label");
    // let acceptKey = bundle.getString("webextPerms.accept.accessKey");
    // let cancelText = bundle.getString("webextPerms.cancel.label");
    // let cancelKey = bundle.getString("webextPerms.cancel.accessKey");
    let acceptText = "Add extension";
    let acceptKey = "A";
    let cancelText = "Cancel";
    let cancelKey = "C";

    if (info.type == "sideload") {
      header = `${name} added`;
      text = "Another program on your computer installed an add-on that may affect your browser.  Please review this add-on's permission requests and choose to Enable or Disable";
      acceptText = "Enable";
      acceptKey = "E";
      cancelText = "Disable";
      cancelKey = "D";
    } else if (info.type == "update") {
      header = "";
      text = `${name} has been updated.  You must approve new permissions before the updated version will install.`;
      acceptText = "Update";
      acceptKey = "U";
    }

    let formatPermission = perm => {
      try {
        // return bundle.getString(`webextPerms.description.${perm}`);
        return `localized description of permission ${perm}`;
      } catch (err) {
        // return bundle.getFormattedString("webextPerms.description.unknown",
        //                                  [perm]);
        return `localized description of unknown permission ${perm}`;
      }
    };

    let formatHostPermission = perm => {
      if (perm == "<all_urls>") {
        // return bundle.getString("webextPerms.hostDescription.allUrls");
        return "localized description of <all_urls> host permission";
      }
      let match = /^[htps*]+:\/\/([^/]+)\//.exec(perm);
      if (!match) {
        throw new Error("Unparseable host permission");
      }
      if (match[1].startsWith("*.")) {
        let domain = match[1].slice(2);
        // return bundle.getFormattedString("webextPerms.hostDescription.wildcard", [domain]);
        return `localized description of wildcard host permission for ${domain}`;
      }

      //  return bundle.getFormattedString("webextPerms.hostDescription.oneSite", [match[1]]);
      return `localized description of single host permission for ${match[1]}`;
    };

    let msgs = [
      ...perms.permissions.map(formatPermission),
      ...perms.hosts.map(formatHostPermission),
    ];

    let rendered = false;
    let popupOptions = {
      hideClose: true,
      popupIconURL: info.icon,
      persistent: true,

      eventCallback(topic) {
        if (topic == "showing") {
          // This check can be removed when bug 1325223 is resolved.
          if (rendered) {
            return false;
          }

          let doc = this.browser.ownerDocument;
          doc.getElementById("addon-webext-perm-header").textContent = header;

          let list = doc.getElementById("addon-webext-perm-list");
          while (list.firstChild) {
            list.firstChild.remove();
          }

          if (text) {
            doc.getElementById("addon-webext-perm-text").textContent = text;
          }

          let listHeaderEl = doc.getElementById("addon-webext-perm-intro");
          listHeaderEl.value = listHeader;
          listHeaderEl.hidden = (msgs.length == 0);

          for (let msg of msgs) {
            let item = doc.createElementNS(HTML_NS, "li");
            item.textContent = msg;
            list.appendChild(item);
          }
          rendered = true;
        } else if (topic == "dismissed") {
          rendered = false;
        } else if (topic == "swapping") {
          rendered = false;
          return true;
        }
        return false;
      },
    };

    return new Promise(resolve => {
      win.PopupNotifications.show(target, "addon-webext-permissions", "",
                                  "addons-notification-icon",
                                  {
                                    label: acceptText,
                                    accessKey: acceptKey,
                                    callback: () => resolve(true),
                                  },
                                  [
                                    {
                                      label: cancelText,
                                      accessKey: cancelKey,
                                      callback: () => resolve(false),
                                    },
                                  ], popupOptions);
    });
  },
};

EventEmitter.decorate(ExtensionsUI);
