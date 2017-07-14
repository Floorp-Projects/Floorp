/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/ContextualIdentityService.jsm");

const containersBundle = Services.strings.createBundle("chrome://browser/locale/preferences/containers.properties");

const defaultContainerIcon = "fingerprint";
const defaultContainerColor = "blue";

let gContainersPane = {

  init() {
    this._list = document.getElementById("containersView");

    document.getElementById("backContainersLink").addEventListener("click", function() {
      gotoPref("privacy");
    });

    this._rebuildView();
  },

  _rebuildView() {
    const containers = ContextualIdentityService.getPublicIdentities();
    while (this._list.firstChild) {
      this._list.firstChild.remove();
    }
    for (let container of containers) {
      let item = document.createElement("richlistitem");
      item.setAttribute("containerName", ContextualIdentityService.getUserContextLabel(container.userContextId));
      item.setAttribute("containerIcon", container.icon);
      item.setAttribute("containerColor", container.color);
      item.setAttribute("userContextId", container.userContextId);

      this._list.appendChild(item);
    }
  },

  onRemoveClick(button) {
    let userContextId = parseInt(button.getAttribute("value"), 10);

    let count = ContextualIdentityService.countContainerTabs(userContextId);
    if (count > 0) {
      let bundlePreferences = document.getElementById("bundlePreferences");

      let title = bundlePreferences.getString("removeContainerAlertTitle");
      let message = PluralForm.get(count, bundlePreferences.getString("removeContainerMsg"))
                              .replace("#S", count)
      let okButton = bundlePreferences.getString("removeContainerOkButton");
      let cancelButton = bundlePreferences.getString("removeContainerButton2");

      let buttonFlags = (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                        (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1);

      let rv = Services.prompt.confirmEx(window, title, message, buttonFlags,
                                         okButton, cancelButton, null, null, {});
      if (rv != 0) {
        return;
      }

      ContextualIdentityService.closeContainerTabs(userContextId);
    }

    ContextualIdentityService.remove(userContextId);
    this._rebuildView();
  },

  onPreferenceClick(button) {
    this.openPreferenceDialog(button.getAttribute("value"));
  },

  onAddButtonClick(button) {
    this.openPreferenceDialog(null);
  },

  openPreferenceDialog(userContextId) {
    let identity = {
      name: "",
      icon: defaultContainerIcon,
      color: defaultContainerColor
    };
    let title;
    if (userContextId) {
      identity = ContextualIdentityService.getPublicIdentityFromId(userContextId);
      // This is required to get the translation string from defaults
      identity.name = ContextualIdentityService.getUserContextLabel(identity.userContextId);
      title = containersBundle.formatStringFromName("containers.updateContainerTitle", [identity.name], 1);
    }

    const params = { userContextId, identity, windowTitle: title };
    gSubDialog.open("chrome://browser/content/preferences/containers.xul",
                     null, params);
  }

};
