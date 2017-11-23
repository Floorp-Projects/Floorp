/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals Mozilla */

(function() {
"use strict";

let onCanSetDefaultBrowserInBackground = () => {
  Mozilla.UITour.getConfiguration("appinfo", config => {
    let canSetInBackGround = config.canSetDefaultBrowserInBackground;
    let btn = document.getElementById("onboarding-tour-default-browser-button");
    btn.setAttribute("data-cansetbg", canSetInBackGround);
    btn.textContent = canSetInBackGround ? btn.getAttribute("data-bg") : btn.getAttribute("data-panel");
  });
};

let onClick = evt => {
  switch (evt.target.id) {
    case "onboarding-tour-addons-button":
      Mozilla.UITour.showHighlight("addons");
      break;
    case "onboarding-tour-customize-button":
      Mozilla.UITour.showHighlight("customize");
      break;
    case "onboarding-tour-default-browser-button":
      Mozilla.UITour.getConfiguration("appinfo", (config) => {
        let isDefaultBrowser = config.defaultBrowser;
        let btn = document.getElementById("onboarding-tour-default-browser-button");
        let msg = document.getElementById("onboarding-tour-is-default-browser-msg");
        let canSetInBackGround = btn.getAttribute("data-cansetbg") === "true";
        if (isDefaultBrowser || canSetInBackGround) {
          btn.classList.add("onboarding-hidden");
          msg.classList.remove("onboarding-hidden");
          if (canSetInBackGround) {
            Mozilla.UITour.setConfiguration("defaultBrowser");
          }
        } else {
          btn.disabled = true;
          Mozilla.UITour.setConfiguration("defaultBrowser");
        }
      });
      break;
    case "onboarding-tour-library-button":
      Mozilla.UITour.showHighlight("library");
      break;
    case "onboarding-tour-private-browsing-button":
      Mozilla.UITour.showHighlight("privateWindow");
      break;
    case "onboarding-tour-singlesearch-button":
      Mozilla.UITour.showMenu("urlbar");
      break;
    case "onboarding-tour-sync-button":
      let emailInput = document.getElementById("onboarding-tour-sync-email-input");
      if (emailInput.checkValidity()) {
        Mozilla.UITour.showFirefoxAccounts(null, emailInput.value);
      }
      break;
    case "onboarding-tour-sync-connect-device-button":
      Mozilla.UITour.showConnectAnotherDevice();
      break;
  }
  let classList = evt.target.classList;
  // On keyboard navigation the target would be .onboarding-tour-item.
  // On mouse clicking the target would be .onboarding-tour-item-container.
  if (classList.contains("onboarding-tour-item") || classList.contains("onboarding-tour-item-container")) {
    Mozilla.UITour.hideHighlight(); // Clean up UITour if a user tries to change to other tours.
  }
};

let overlay = document.getElementById("onboarding-overlay");
overlay.addEventListener("submit", e => e.preventDefault());
overlay.addEventListener("click", onClick);
overlay.addEventListener("keypress", e => {
  let { target, key } = e;
  let classList = target.classList;
  if ((key == " " || key == "Enter") &&
      // On keyboard navigation the target would be .onboarding-tour-item.
      // On mouse clicking the target would be .onboarding-tour-item-container.
      (classList.contains("onboarding-tour-item") || classList.contains("onboarding-tour-item-container"))) {
    Mozilla.UITour.hideHighlight(); // Clean up UITour if a user tries to change to other tours.
  }
});
let overlayObserver = new MutationObserver(mutations => {
  if (!overlay.classList.contains("onboarding-opened")) {
    Mozilla.UITour.hideHighlight(); // Clean up UITour if a user tries to close the dialog.
  }
});
overlayObserver.observe(overlay, { attributes: true });
document.getElementById("onboarding-overlay-button").addEventListener("Agent:Destroy", () => Mozilla.UITour.hideHighlight());
document.addEventListener("Agent:CanSetDefaultBrowserInBackground", onCanSetDefaultBrowserInBackground);

})();
