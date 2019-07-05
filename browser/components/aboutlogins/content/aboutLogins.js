/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { recordTelemetryEvent } from "./aboutLoginsUtils.js";

let gElements = {};

document.addEventListener(
  "DOMContentLoaded",
  () => {
    gElements.loginList = document.querySelector("login-list");
    gElements.loginItem = document.querySelector("login-item");
    gElements.loginFilter = document.querySelector("login-filter");
    gElements.newLoginButton = document.querySelector("#create-login-button");

    let { searchParams } = new URL(document.location);
    if (searchParams.get("filter")) {
      gElements.loginFilter.value = searchParams.get("filter");
    }

    gElements.newLoginButton.addEventListener("click", () => {
      window.dispatchEvent(new CustomEvent("AboutLoginsCreateLogin"));
      recordTelemetryEvent({ object: "new_login", method: "new" });
    });

    document.dispatchEvent(
      new CustomEvent("AboutLoginsInit", { bubbles: true })
    );

    gElements.loginFilter.focus();
  },
  { once: true }
);

window.addEventListener("AboutLoginsChromeToContent", event => {
  switch (event.detail.messageType) {
    case "AllLogins": {
      gElements.loginList.setLogins(event.detail.value);
      break;
    }
    case "LoginAdded": {
      gElements.loginList.loginAdded(event.detail.value);
      gElements.loginItem.loginAdded(event.detail.value);
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
