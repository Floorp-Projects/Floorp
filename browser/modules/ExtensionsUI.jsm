/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["ExtensionsUI"];

Cu.import("resource://gre/modules/Services.jsm");

const DEFAULT_EXENSION_ICON = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

const HTML_NS = "http://www.w3.org/1999/xhtml";

this.ExtensionsUI = {
  init() {
    Services.obs.addObserver(this, "webextension-permission-prompt", false);
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

      this.showPermissionsPrompt(target, info).then(answer => {
        Services.obs.notifyObservers(subject, "webextension-permission-response",
                                     JSON.stringify(answer));
      });
    }
  },

  showPermissionsPrompt(target, info) {
    let perms = info.addon.userPermissions;
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
    let listHeader = "It can:";

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

    // let acceptText = bundle.getString("webextPerms.accept.label");
    // let acceptKey = bundle.getString("webextPerms.accept.accessKey");
    // let cancelText = bundle.getString("webextPerms.cancel.label");
    // let cancelKey = bundle.getString("webextPerms.cancel.accessKey");
    let acceptText = "Add extension";
    let acceptKey = "A";
    let cancelText = "Cancel";
    let cancelKey = "C";

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

          let listHeaderEl = doc.getElementById("addon-webext-perm-text");
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
