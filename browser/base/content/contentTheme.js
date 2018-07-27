/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{

function _isTextColorDark(r, g, b) {
  return (0.2125 * r + 0.7154 * g + 0.0721 * b) <= 110;
}

const inContentVariableMap = [
  ["--newtab-background-color", {
    lwtProperty: "ntp_background"
  }],
  ["--newtab-text-primary-color", {
    lwtProperty: "ntp_text",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels) {
        element.removeAttribute("lwt-newtab");
        element.removeAttribute("lwt-newtab-brighttext");
        return null;
      }

      element.setAttribute("lwt-newtab", "true");
      const {r, g, b, a} = rgbaChannels;
      if (!_isTextColorDark(r, g, b)) {
        element.setAttribute("lwt-newtab-brighttext", "true");
      }

      return `rgba(${r}, ${g}, ${b}, ${a})`;
    },
  }],
];

/**
 * ContentThemeController handles theme updates sent by the frame script.
 * To be able to use ContentThemeController, you must add your page to the whitelist
 * in LightweightThemeChildListener.jsm
 */
const ContentThemeController = {
  /**
   * Tell the frame script that the page supports theming, and watch for updates
   * from the frame script.
   */
  init() {
    addEventListener("LightweightTheme:Set", this);
  },

  /**
   * Handle theme updates from the frame script.
   * @param {Object} event object containing the theme update.
   */
  handleEvent({ type, detail }) {
    if (type == "LightweightTheme:Set") {
      let {data} = detail;
      if (!data) {
        data = {};
      }
      this._setProperties(document.body, data);
    }
  },

  /**
   * Set a CSS variable to a given value
   * @param {Element} elem The element where the CSS variable should be added.
   * @param {string} variableName The CSS variable to set.
   * @param {string} value The new value of the CSS variable.
   */
  _setProperty(elem, variableName, value) {
    if (value) {
      elem.style.setProperty(variableName, value);
    } else {
      elem.style.removeProperty(variableName);
    }
  },

  /**
   * Apply theme data to an element
   * @param {Element} root The element where the properties should be applied.
   * @param {Object} themeData The theme data.
   */
  _setProperties(elem, themeData) {
    for (let [cssVarName, definition] of inContentVariableMap) {
      const {
        lwtProperty,
        processColor,
      } = definition;
      let value = themeData[lwtProperty];

      if (processColor) {
        value = processColor(value, document.body);
      } else if (value) {
        const {r, g, b, a} = value;
        value = `rgba(${r}, ${g}, ${b}, ${a})`;
      }

      this._setProperty(elem, cssVarName, value);
    }
  },
};
ContentThemeController.init();

}
