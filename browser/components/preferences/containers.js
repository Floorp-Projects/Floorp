/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/ContextualIdentityService.jsm");

const containersBundle = Services.strings.createBundle("chrome://browser/locale/preferences/containers.properties");

const HTMLNS = "http://www.w3.org/1999/xhtml";

let gContainersManager = {
  icons: [
    "fingerprint",
    "briefcase",
    "dollar",
    "cart",
    "circle"
  ],

  colors: [
    "blue",
    "turquoise",
    "green",
    "yellow",
    "orange",
    "red",
    "pink",
    "purple"
  ],

  onLoad() {
    let params = window.arguments[0] || {};
    this.init(params);
  },

  init(aParams) {
    this.userContextId = aParams.userContextId || null;
    this.identity = aParams.identity;

    if (aParams.windowTitle) {
      document.title = aParams.windowTitle;
    }

    const iconWrapper = document.getElementById("iconWrapper");
    iconWrapper.appendChild(this.createIconButtons());

    const colorWrapper = document.getElementById("colorWrapper");
    colorWrapper.appendChild(this.createColorSwatches());

    if (this.identity.name) {
      const name = document.getElementById("name");
      name.value = this.identity.name;
      this.checkForm();
    }

    this.setLabelsMinWidth();

    // This is to prevent layout jank caused by the svgs and outlines rendering at different times
    document.getElementById("containers-content").removeAttribute("hidden");
  },

  setLabelsMinWidth() {
    const labelMinWidth = containersBundle.GetStringFromName("containers.labelMinWidth");
    const labels = [
      document.getElementById("nameLabel"),
      document.getElementById("iconLabel"),
      document.getElementById("colorLabel")
    ];
    for (let label of labels) {
      label.style.minWidth = labelMinWidth;
    }
  },

  uninit() {
  },

  // Check if name string as to if the form can be submitted
  checkForm() {
    const name = document.getElementById("name");
    let btnApplyChanges = document.getElementById("btnApplyChanges");
    if (!name.value) {
      btnApplyChanges.setAttribute("disabled", true);
    } else {
      btnApplyChanges.removeAttribute("disabled");
    }
  },

  createIconButtons(defaultIcon) {
    let radiogroup = document.createElement("radiogroup");
    radiogroup.setAttribute("id", "icon");
    radiogroup.className = "icon-buttons radio-buttons";

    for (let icon of this.icons) {
      let iconSwatch = document.createElement("radio");
      iconSwatch.id = "iconbutton-" + icon;
      iconSwatch.name = "icon";
      iconSwatch.type = "radio";
      iconSwatch.value = icon;

      if (this.identity.icon && this.identity.icon == icon) {
        iconSwatch.setAttribute("selected", true);
      }

      iconSwatch.setAttribute("label",
        containersBundle.GetStringFromName(`containers.${icon}.label`));
      let iconElement = document.createElement("hbox");
      iconElement.className = 'userContext-icon';
      iconElement.setAttribute("data-identity-icon", icon);

      iconSwatch.appendChild(iconElement);
      radiogroup.appendChild(iconSwatch);
    }

    return radiogroup;
  },

  createColorSwatches(defaultColor) {
    let radiogroup = document.createElement("radiogroup");
    radiogroup.setAttribute("id", "color");
    radiogroup.className = "radio-buttons";

    for (let color of this.colors) {
      let colorSwatch = document.createElement("radio");
      colorSwatch.id = "colorswatch-" + color;
      colorSwatch.name = "color";
      colorSwatch.type = "radio";
      colorSwatch.value = color;

      if (this.identity.color && this.identity.color == color) {
        colorSwatch.setAttribute("selected", true);
      }

      colorSwatch.setAttribute("label",
        containersBundle.GetStringFromName(`containers.${color}.label`));
      let iconElement = document.createElement("hbox");
      iconElement.className = 'userContext-icon';
      iconElement.setAttribute("data-identity-icon", "circle");
      iconElement.setAttribute("data-identity-color", color);

      colorSwatch.appendChild(iconElement);
      radiogroup.appendChild(colorSwatch);
    }
    return radiogroup;
  },

  onApplyChanges() {
    let icon = document.getElementById("icon").value;
    let color = document.getElementById("color").value;
    let name = document.getElementById("name").value;

    if (this.icons.indexOf(icon) == -1) {
      throw "Internal error. The icon value doesn't match.";
    }

    if (this.colors.indexOf(color) == -1) {
      throw "Internal error. The color value doesn't match.";
    }

    if (this.userContextId) {
      ContextualIdentityService.update(this.userContextId,
        name,
        icon,
        color);
    } else {
      ContextualIdentityService.create(name,
        icon,
        color);
    }
    window.parent.location.reload()
  },

  onWindowKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  }
}
