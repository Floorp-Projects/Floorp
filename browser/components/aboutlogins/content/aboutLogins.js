/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The init code isn't wrapped in a DOMContentLoaded/load event listener so the
// page works properly when restored from session restore.
const gElements = {
  fxAccountsButton: document.querySelector("fxaccounts-button"),
  loginList: document.querySelector("login-list"),
  loginIntro: document.querySelector("login-intro"),
  loginItem: document.querySelector("login-item"),
  loginFilter: document.querySelector("login-filter"),
  // loginFooter is nested inside of loginItem
  get loginFooter() {
    return this.loginItem.shadowRoot.querySelector("login-footer");
  },
};

let numberOfLogins = 0;

let { searchParams } = new URL(document.location);
if (searchParams.get("filter")) {
  gElements.loginFilter.value = searchParams.get("filter");
}

gElements.loginFilter.focus();

function updateNoLogins() {
  document.documentElement.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.loginList.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.loginItem.classList.toggle("no-logins", numberOfLogins == 0);
}

window.addEventListener("AboutLoginsChromeToContent", event => {
  switch (event.detail.messageType) {
    case "AllLogins": {
      gElements.loginList.setLogins(event.detail.value);
      numberOfLogins = event.detail.value.length;
      updateNoLogins();
      break;
    }
    case "LocalizeBadges": {
      gElements.loginFooter.showStoreIconsForLocales(event.detail.value);
      break;
    }
    case "LoginAdded": {
      gElements.loginList.loginAdded(event.detail.value);
      gElements.loginItem.loginAdded(event.detail.value);
      numberOfLogins++;
      updateNoLogins();
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
      numberOfLogins--;
      updateNoLogins();
      break;
    }
    case "SendFavicons": {
      gElements.loginList.addFavicons(event.detail.value);
      break;
    }
    case "ShowLoginItemError": {
      gElements.loginItem.showLoginItemError(event.detail.value);
      break;
    }
    case "SyncState": {
      gElements.fxAccountsButton.updateState(event.detail.value);
      gElements.loginFooter.hidden = event.detail.value.hideMobileFooter;
      break;
    }
    case "UpdateBreaches": {
      gElements.loginList.updateBreaches(event.detail.value);
      gElements.loginItem.updateBreaches(event.detail.value);
      break;
    }
  }
});

document.dispatchEvent(new CustomEvent("AboutLoginsInit", { bubbles: true }));
