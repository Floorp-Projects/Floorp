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
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "WEBEXT_PERMISSION_PROMPTS",
                                      "extensions.webextPermissionPrompts", false);

const DEFAULT_EXTENSION_ICON = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

const HTML_NS = "http://www.w3.org/1999/xhtml";

this.ExtensionsUI = {
  sideloaded: new Set(),
  updates: new Set(),

  init() {
    Services.obs.addObserver(this, "webextension-permission-prompt", false);
    Services.obs.addObserver(this, "webextension-update-permissions", false);
    Services.obs.addObserver(this, "webextension-install-notify", false);

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
    } else if (topic == "webextension-install-notify") {
      let {target, addon} = subject.wrappedJSObject;
      this.showInstallNotification(target, addon);
    }
  },

  // Escape &, <, and > characters in a string so that it may be
  // injected as part of raw markup.
  _sanitizeName(name) {
    return name.replace(/&/g, "&amp;")
               .replace(/</g, "&lt;")
               .replace(/>/g, "&gt;");
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
    name = this._sanitizeName(name);

    let addonLabel = `<label class="addon-webext-name">${name}</label>`;
    let bundle = win.gNavigatorBundle;

    let header = bundle.getFormattedString("webextPerms.header", [addonLabel]);
    let text = "";
    let listIntro = bundle.getString("webextPerms.listIntro");

    let acceptText = bundle.getString("webextPerms.add.label");
    let acceptKey = bundle.getString("webextPerms.add.accessKey");
    let cancelText = bundle.getString("webextPerms.cancel.label");
    let cancelKey = bundle.getString("webextPerms.cancel.accessKey");

    if (info.type == "sideload") {
      header = bundle.getFormattedString("webextPerms.sideloadHeader", [addonLabel]);
      text = bundle.getString("webextPerms.sideloadText");
      acceptText = bundle.getString("webextPerms.sideloadEnable.label");
      acceptKey = bundle.getString("webextPerms.sideloadEnable.accessKey");
      cancelText = bundle.getString("webextPerms.sideloadDisable.label");
      cancelKey = bundle.getString("webextPerms.sideloadDisable.accessKey");
    } else if (info.type == "update") {
      header = "";
      text = bundle.getFormattedString("webextPerms.updateText", [addonLabel]);
      acceptText = bundle.getString("webextPerms.updateAccept.label");
      acceptKey = bundle.getString("webextPerms.updateAccept.accessKey");
    }

    let msgs = [];
    for (let permission of perms.permissions) {
      let key = `webextPerms.description.${permission}`;
      if (permission == "nativeMessaging") {
        let brandBundle = win.document.getElementById("bundle_brand");
        let appName = brandBundle.getString("brandShortName");
        msgs.push(bundle.getFormattedString(key, [appName]));
      } else {
        try {
          msgs.push(bundle.getString(key));
        } catch (err) {
          // We deliberately do not include all permissions in the prompt.
          // So if we don't find one then just skip it.
        }
      }
    }

    let allUrls = false, wildcards = [], sites = [];
    for (let permission of perms.hosts) {
      if (permission == "<all_urls>") {
        allUrls = true;
        break;
      }
      let match = /^[htps*]+:\/\/([^/]+)\//.exec(permission);
      if (!match) {
        throw new Error("Unparseable host permission");
      }
      if (match[1] == "*") {
        allUrls = true;
      } else if (match[1].startsWith("*.")) {
        wildcards.push(match[1].slice(2));
      } else {
        sites.push(match[1]);
      }
    }

    if (allUrls) {
      msgs.push(bundle.getString("webextPerms.hostDescription.allUrls"));
    } else {
      // Formats a list of host permissions.  If we have 4 or fewer, display
      // them all, otherwise display the first 3 followed by an item that
      // says "...plus N others"
      function format(list, itemKey, moreKey) {
        function formatItems(items) {
          msgs.push(...items.map(item => bundle.getFormattedString(itemKey, [item])));
        }
        if (list.length < 5) {
          formatItems(list);
        } else {
          formatItems(list.slice(0, 3));

          let remaining = list.length - 3;
          msgs.push(PluralForm.get(remaining, bundle.getString(moreKey))
                              .replace("#1", remaining));
        }
      }

      format(wildcards, "webextPerms.hostDescription.wildcard",
             "webextPerms.hostDescription.tooManyWildcards");
      format(sites, "webextPerms.hostDescription.oneSite",
             "webextPerms.hostDescription.tooManySites");
    }

    let popupOptions = {
      hideClose: true,
      popupIconURL: info.icon,
      persistent: true,

      eventCallback(topic) {
        if (topic == "showing") {
          let doc = this.browser.ownerDocument;
          doc.getElementById("addon-webext-perm-header").innerHTML = header;

          if (text) {
            doc.getElementById("addon-webext-perm-text").innerHTML = text;
          }

          let listIntroEl = doc.getElementById("addon-webext-perm-intro");
          listIntroEl.value = listIntro;
          listIntroEl.hidden = (msgs.length == 0);

          let list = doc.getElementById("addon-webext-perm-list");
          while (list.firstChild) {
            list.firstChild.remove();
          }

          for (let msg of msgs) {
            let item = doc.createElementNS(HTML_NS, "li");
            item.textContent = msg;
            list.appendChild(item);
          }
        } else if (topic == "swapping") {
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

  showInstallNotification(target, addon) {
    let win = target.ownerGlobal;
    let popups = win.PopupNotifications;

    let name = this._sanitizeName(addon.name);
    let addonLabel = `<label class="addon-webext-name">${name}</label>`;
    let addonIcon = '<image class="addon-addon-icon"/>';
    let toolbarIcon = '<image class="addon-toolbar-icon"/>';

    let brandBundle = win.document.getElementById("bundle_brand");
    let appName = brandBundle.getString("brandShortName");

    let bundle = win.gNavigatorBundle;
    let msg1 = bundle.getFormattedString("addonPostInstall.message1",
                                         [addonLabel, appName]);
    let msg2 = bundle.getFormattedString("addonPostInstall.message2",
                                         [addonLabel, addonIcon, toolbarIcon]);

    let action = {
      label: bundle.getString("addonPostInstall.okay.label"),
      accessKey: bundle.getString("addonPostInstall.okay.key"),
      callback: () => {},
    };

    let options = {
      hideClose: true,
      popupIconURL: addon.iconURL || DEFAULT_EXTENSION_ICON,
      eventCallback(topic) {
        if (topic == "showing") {
          let doc = this.browser.ownerDocument;
          doc.getElementById("addon-installed-notification-header")
             .innerHTML = msg1;
          doc.getElementById("addon-installed-notification-message")
             .innerHTML = msg2;
        }
      }
    };

    popups.show(target, "addon-installed", "", "addons-notification-icon",
                action, null, options);
  },
};

EventEmitter.decorate(ExtensionsUI);
