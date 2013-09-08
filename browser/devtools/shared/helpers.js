/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");
Cu.import("resource://gre/modules/Services.jsm");

let { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
XPCOMUtils.defineLazyGetter(this, "PlatformKeys", function() {
  return Services.strings.createBundle(
    "chrome://global-platform/locale/platformKeys.properties");
});

/**
  * Prettifies the modifier keys for an element.
  *
  * @param Node aElemKey
  *        The key element to get the modifiers from.
  * @param boolean aAllowCloverleaf
  *        Pass true to use the cloverleaf symbol instead of a descriptive string.
  * @return string
  *         A prettified and properly separated modifier keys string.
  */
exports.prettyKey = function Helpers_prettyKey(aElemKey, aAllowCloverleaf) {
  let elemString = "";
  let elemMod = aElemKey.getAttribute("modifiers");

  if (elemMod.match("accel")) {
    if (Services.appinfo.OS == "Darwin") {
      // XXX bug 779642 Use "Cmd-" literal vs. cloverleaf meta-key until
      // Orion adds variable height lines.
      if (!aAllowCloverleaf) {
        elemString += "Cmd-";
      } else {
        elemString += PlatformKeys.GetStringFromName("VK_META") +
          PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
      }
    } else {
      elemString += PlatformKeys.GetStringFromName("VK_CONTROL") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }
  }
  if (elemMod.match("access")) {
    if (Services.appinfo.OS == "Darwin") {
      elemString += PlatformKeys.GetStringFromName("VK_CONTROL") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    } else {
      elemString += PlatformKeys.GetStringFromName("VK_ALT") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }
  }
  if (elemMod.match("shift")) {
    elemString += PlatformKeys.GetStringFromName("VK_SHIFT") +
      PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
  }
  if (elemMod.match("alt")) {
    elemString += PlatformKeys.GetStringFromName("VK_ALT") +
      PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
  }
  if (elemMod.match("ctrl") || elemMod.match("control")) {
    elemString += PlatformKeys.GetStringFromName("VK_CONTROL") +
      PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
  }
  if (elemMod.match("meta")) {
    elemString += PlatformKeys.GetStringFromName("VK_META") +
      PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
  }

  return elemString +
    (aElemKey.getAttribute("keycode").replace(/^.*VK_/, "") ||
     aElemKey.getAttribute("key")).toUpperCase();
}
