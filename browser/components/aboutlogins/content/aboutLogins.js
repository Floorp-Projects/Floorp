/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  recordTelemetryEvent,
  setKeyboardAccessForNonDialogElements,
} from "./aboutLoginsUtils.js";

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

function updateNoLogins() {
  document.documentElement.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.loginList.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.loginItem.classList.toggle("no-logins", numberOfLogins == 0);
}

function handleAllLogins(logins) {
  gElements.loginList.setLogins(logins);
  numberOfLogins = logins.length;
  updateNoLogins();
}

function handleSyncState(syncState) {
  gElements.fxAccountsButton.updateState(syncState);
  gElements.loginFooter.hidden = syncState.hideMobileFooter;
  gElements.loginIntro.updateState(syncState);
}

window.addEventListener("AboutLoginsChromeToContent", event => {
  switch (event.detail.messageType) {
    case "AllLogins": {
      document.documentElement.classList.remove(
        "master-password-auth-required"
      );
      setKeyboardAccessForNonDialogElements(true);
      handleAllLogins(event.detail.value);
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
      // The loginRemoved function of loginItem needs to be called before
      // the one in loginList since it will remove the editing. So that the
      // discard dialog won't show up if we delete a login after edit it.
      gElements.loginItem.loginRemoved(event.detail.value);
      gElements.loginList.loginRemoved(event.detail.value);
      numberOfLogins--;
      updateNoLogins();
      break;
    }
    case "MasterPasswordAuthRequired":
      document.documentElement.classList.add("master-password-auth-required");
      setKeyboardAccessForNonDialogElements(false);
      break;
    case "SendFavicons": {
      gElements.loginList.addFavicons(event.detail.value);
      break;
    }
    case "SetBreaches": {
      gElements.loginList.setBreaches(event.detail.value);
      gElements.loginItem.setBreaches(event.detail.value);
      break;
    }
    case "SetVulnerableLogins": {
      gElements.loginList.setVulnerableLogins(event.detail.value);
      gElements.loginItem.setVulnerableLogins(event.detail.value);
      break;
    }
    case "Setup": {
      handleAllLogins(event.detail.value.logins);
      gElements.loginFooter.showStoreIconsForLocales(
        event.detail.value.selectedBadgeLanguages
      );
      handleSyncState(event.detail.value.syncState);
      gElements.loginList.setSortDirection(event.detail.value.selectedSort);
      document.documentElement.classList.add("initialized");
      gElements.loginList.classList.add("initialized");
      break;
    }
    case "ShowLoginItemError": {
      gElements.loginItem.showLoginItemError(event.detail.value);
      break;
    }
    case "SyncState": {
      handleSyncState(event.detail.value);
      break;
    }
    case "UpdateBreaches": {
      gElements.loginList.updateBreaches(event.detail.value);
      gElements.loginItem.updateBreaches(event.detail.value);
      break;
    }
    case "UpdateVulnerableLogins": {
      gElements.loginList.updateVulnerableLogins(event.detail.value);
      gElements.loginItem.updateVulnerableLogins(event.detail.value);
      break;
    }
  }
});

// Begin code that executes on page load.

let searchParamsChanged = false;
let { protocol, pathname, searchParams } = new URL(document.location);

recordTelemetryEvent({
  method: "open_management",
  object: searchParams.get("entryPoint") || "direct",
});

if (searchParams.has("entryPoint")) {
  // Remove this parameter from the URL (after recording above) to make it
  // cleaner for bookmarking and switch-to-tab and so that bookmarked values
  // don't skew telemetry.
  searchParams.delete("entryPoint");
  searchParamsChanged = true;
}

if (searchParams.has("filter")) {
  let filter = searchParams.get("filter");
  if (!filter) {
    // Remove empty `filter` params to give a cleaner URL for bookmarking and
    // switch-to-tab
    searchParams.delete("filter");
    searchParamsChanged = true;
  }
}

if (searchParamsChanged) {
  let newURL = protocol + pathname;
  let params = searchParams.toString();
  if (params) {
    newURL += "?" + params;
  }
  // This redirect doesn't stop this script from running so ensure you guard
  // later code if it shouldn't run before and after the redirect.
  window.location.replace(newURL);
} else if (searchParams.has("filter")) {
  // This must be after the `location.replace` so it doesn't cause telemetry to
  // record a filter event before the navigation to clean the URL.
  gElements.loginFilter.value = searchParams.get("filter");
}

if (!searchParamsChanged) {
  gElements.loginFilter.focus();
  document.dispatchEvent(new CustomEvent("AboutLoginsInit", { bubbles: true }));
}
