/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Preferences.addAll([
  { id: "privacy.resistFingerprinting", type: "bool"},
  { id: "privacy.resistFingerprinting.autoDeclineNoUserInputCanvasPrompts", type: "bool"},
  { id: "webgl.disabled", type: "bool"},
  { id: "media.peerconnection.enabled", type: "bool"},
]);

const addonStatus = async(addonID, className) => {
  const addon = await AddonManager.getAddonByID(addonID);
  if (addon !== null) {
    const addontag = document.createElement("style");
    addontag.id = className;
    addontag.innerText = `
      .${className} {
        display: none !important;
      }`;
    document.head.insertAdjacentElement("beforeend", addontag);
  }
};
  
  addonStatus("uBlock0@raymondhill.net", "uBlockOrigin");
  
  window.setTimeout(() => {
    const UBTag = document.getElementById("uBlockOrigin") != null;
  
    if (UBTag) {
      document.getElementById("blockmoretrackers").style.display = "none";
    }
}, 1000);