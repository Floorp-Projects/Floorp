/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/ContextualIdentityService.jsm");

const defaultContainerIcon = "fingerprint";
const defaultContainerColor = "blue";

let gContainersPane = {

  init() {
    this._list = document.getElementById("containersView");

    document.getElementById("backContainersLink").addEventListener("click", function(event) {
      if (event.button == 0) {
        gotoPref("general");
      }
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

      let outer = document.createElement("hbox");
      outer.setAttribute("flex", 1);
      outer.setAttribute("align", "center");
      item.appendChild(outer);

      let userContextIcon = document.createElement("hbox");
      userContextIcon.className = "userContext-icon";
      userContextIcon.setAttribute("width", 24);
      userContextIcon.setAttribute("height", 24);
      userContextIcon.setAttribute("data-identity-icon", container.icon);
      userContextIcon.setAttribute("data-identity-color", container.color);
      outer.appendChild(userContextIcon);

      let label = document.createElement("label");
      label.setAttribute("flex", 1);
      label.setAttribute("crop", "end");
      label.textContent = ContextualIdentityService.getUserContextLabel(container.userContextId);
      outer.appendChild(label);

      let containerButtons = document.createElement("hbox");
      containerButtons.className = "container-buttons";
      containerButtons.setAttribute("flex", 1);
      containerButtons.setAttribute("align", "right");
      item.appendChild(containerButtons);

      let prefsButton = document.createElement("button");
      prefsButton.setAttribute("oncommand", "gContainersPane.onPreferenceCommand(event.originalTarget)");
      prefsButton.setAttribute("value", container.userContextId);
      document.l10n.setAttributes(prefsButton, "containers-preferences-button");
      containerButtons.appendChild(prefsButton);

      let removeButton = document.createElement("button");
      removeButton.setAttribute("oncommand", "gContainersPane.onRemoveCommand(event.originalTarget)");
      removeButton.setAttribute("value", container.userContextId);
      document.l10n.setAttributes(removeButton, "containers-remove-button");
      containerButtons.appendChild(removeButton);

      this._list.appendChild(item);
    }
  },

  async onRemoveCommand(button) {
    let userContextId = parseInt(button.getAttribute("value"), 10);

    let count = ContextualIdentityService.countContainerTabs(userContextId);
    if (count > 0) {
      let [title, message, okButton, cancelButton] = await document.l10n.formatValues([
        ["containers-remove-alert-title"],
        ["containers-remove-alert-msg", { count }],
        ["containers-remove-ok-button"],
        ["containers-remove-cancel-button"]
      ]);

      let buttonFlags = (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                        (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1);

      let rv = Services.prompt.confirmEx(window, title, message, buttonFlags,
                                         okButton, cancelButton, null, null, {});
      if (rv != 0) {
        return;
      }

      await ContextualIdentityService.closeContainerTabs(userContextId);
    }

    ContextualIdentityService.remove(userContextId);
    this._rebuildView();
  },

  onPreferenceCommand(button) {
    this.openPreferenceDialog(button.getAttribute("value"));
  },

  onAddButtonCommand(button) {
    this.openPreferenceDialog(null);
  },

  openPreferenceDialog(userContextId) {
    let identity = {
      name: "",
      icon: defaultContainerIcon,
      color: defaultContainerColor
    };
    if (userContextId) {
      identity = ContextualIdentityService.getPublicIdentityFromId(userContextId);
      identity.name = ContextualIdentityService.getUserContextLabel(identity.userContextId);
    }

    const params = { userContextId, identity };
    gSubDialog.open("chrome://browser/content/preferences/containers.xul",
                     null, params);
  }

};
