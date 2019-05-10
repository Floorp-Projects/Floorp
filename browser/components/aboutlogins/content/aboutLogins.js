/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gElements = {};

document.addEventListener("DOMContentLoaded", () => {
  gElements.loginList = document.querySelector("login-list");
  gElements.loginItem = document.querySelector("login-item");

  document.dispatchEvent(new CustomEvent("AboutLoginsInit", {bubbles: true}));
}, {once: true});

window.addEventListener("AboutLoginsChromeToContent", event => {
  switch (event.detail.messageType) {
    case "AllLogins": {
      gElements.loginList.setLogins(event.detail.value);
      break;
    }
    case "LoginAdded": {
      gElements.loginList.loginAdded(event.detail.value);
      break;
    }
    case "LoginModified": {
      gElements.loginList.loginModified(event.detail.value);
      gElements.loginItem.loginModified(event.detail.value);
      break;
    }
    case "LoginRemoved": {
      gElements.loginList.loginRemoved(event.detail.value);
      gElements.loginItem.loginRemoved(event.detail.value);
      break;
    }
  }
});
