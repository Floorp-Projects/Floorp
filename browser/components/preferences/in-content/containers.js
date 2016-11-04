/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/ContextualIdentityService.jsm");

const containersBundle = Services.strings.createBundle("chrome://browser/locale/preferences/containers.properties");

const defaultContainerIcon = "fingerprint";
const defaultContainerColor = "blue";

let gContainersPane = {

  init() {
    this._list = document.getElementById("containersView");

    document.getElementById("backContainersLink").addEventListener("click", function () {
      gotoPref("privacy");
    });

    this._rebuildView();
  },

  _rebuildView() {
    const containers = ContextualIdentityService.getIdentities();
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
    let userContextId = button.getAttribute("value");
    ContextualIdentityService.remove(userContextId);
    this._rebuildView();
  },
  onPeferenceClick(button) {
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
      identity = ContextualIdentityService.getIdentityFromId(userContextId);
      // This is required to get the translation string from defaults
      identity.name = ContextualIdentityService.getUserContextLabel(identity.userContextId);
      title = containersBundle.formatStringFromName("containers.updateContainerTitle", [identity.name], 1);
    }

    const params = { userContextId, identity, windowTitle: title };
    gSubDialog.open("chrome://browser/content/preferences/containers.xul",
                     null, params);
  }

};
