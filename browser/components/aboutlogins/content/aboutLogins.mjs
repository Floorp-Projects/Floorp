/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  recordTelemetryEvent,
  setKeyboardAccessForNonDialogElements,
} from "./aboutLoginsUtils.mjs";

// The init code isn't wrapped in a DOMContentLoaded/load event listener so the
// page works properly when restored from session restore.
const gElements = {
  fxAccountsButton: document.querySelector("fxaccounts-button"),
  loginList: document.querySelector("login-list"),
  loginIntro: document.querySelector("login-intro"),
  loginItem: document.querySelector("login-item"),
  loginFilter: document.querySelector("login-filter"),
  menuButton: document.querySelector("menu-button"),
  // removeAllLogins button is nested inside of menuButton
  get removeAllButton() {
    return this.menuButton.shadowRoot.querySelector(
      ".menuitem-remove-all-logins"
    );
  },
};

let numberOfLogins = 0;

function updateNoLogins() {
  document.documentElement.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.loginList.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.loginItem.classList.toggle("no-logins", numberOfLogins == 0);
  gElements.removeAllButton.disabled = numberOfLogins == 0;
}

function handleAllLogins(logins) {
  gElements.loginList.setLogins(logins);
  numberOfLogins = logins.length;
  updateNoLogins();
}

let fxaLoggedIn = null;
let passwordSyncEnabled = null;

function handleSyncState(syncState) {
  gElements.fxAccountsButton.updateState(syncState);
  gElements.loginIntro.updateState(syncState);
  fxaLoggedIn = syncState.loggedIn;
  passwordSyncEnabled = syncState.passwordSyncEnabled;
}

window.addEventListener("AboutLoginsChromeToContent", event => {
  switch (event.detail.messageType) {
    case "AllLogins": {
      document.documentElement.classList.remove(
        "primary-password-auth-required"
      );
      setKeyboardAccessForNonDialogElements(true);
      handleAllLogins(event.detail.value);
      break;
    }
    case "ImportPasswordsDialog": {
      let dialog = document.querySelector("import-summary-dialog");
      let options = {
        logins: event.detail.value,
      };
      dialog.show(options);
      break;
    }
    case "ImportPasswordsErrorDialog": {
      let dialog = document.querySelector("import-error-dialog");
      dialog.show(event.detail.value);
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
    case "PrimaryPasswordAuthRequired": {
      document.documentElement.classList.add("primary-password-auth-required");
      setKeyboardAccessForNonDialogElements(false);
      break;
    }
    case "RemaskPassword": {
      window.dispatchEvent(new CustomEvent("AboutLoginsRemaskPassword"));
      break;
    }
    case "RemoveAllLogins": {
      handleAllLogins(event.detail.value);
      document.documentElement.classList.remove("login-selected");
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

window.addEventListener("AboutLoginsRemoveAllLoginsDialog", () => {
  let loginItem = document.querySelector("login-item");
  let options = {};
  if (fxaLoggedIn && passwordSyncEnabled) {
    options.title = "about-logins-confirm-remove-all-sync-dialog-title";
    options.message = "about-logins-confirm-remove-all-sync-dialog-message";
  } else {
    options.title = "about-logins-confirm-remove-all-dialog-title";
    options.message = "about-logins-confirm-remove-all-dialog-message";
  }
  options.confirmCheckboxLabel =
    "about-logins-confirm-remove-all-dialog-checkbox-label";
  options.confirmButtonLabel =
    "about-logins-confirm-remove-all-dialog-confirm-button-label";
  options.count = numberOfLogins;

  let dialog = document.querySelector("remove-logins-dialog");
  let dialogPromise = dialog.show(options);
  try {
    dialogPromise.then(
      () => {
        if (loginItem.dataset.isNewLogin) {
          // Bug 1681042 - Resetting the form prevents a double confirmation dialog since there
          // may be pending changes in the new login.
          loginItem.resetForm();
          window.dispatchEvent(new CustomEvent("AboutLoginsClearSelection"));
        } else if (loginItem.dataset.editing) {
          loginItem._toggleEditing();
        }
        window.document.documentElement.classList.remove("login-selected");
        let removeAllEvt = new CustomEvent("AboutLoginsRemoveAllLogins", {
          bubbles: true,
        });
        window.dispatchEvent(removeAllEvt);
      },
      () => {}
    );
  } catch (e) {
    if (e != undefined) {
      throw e;
    }
  }
});

window.addEventListener("AboutLoginsExportPasswordsDialog", async event => {
  recordTelemetryEvent({
    object: "export",
    method: "mgmt_menu_item_used",
  });
  let dialog = document.querySelector("confirmation-dialog");
  let options = {
    title: "about-logins-confirm-export-dialog-title",
    message: "about-logins-confirm-export-dialog-message",
    confirmButtonLabel: "about-logins-confirm-export-dialog-confirm-button",
  };
  try {
    await dialog.show(options);
    document.dispatchEvent(
      new CustomEvent("AboutLoginsExportPasswords", { bubbles: true })
    );
  } catch (ex) {
    // The user cancelled the dialog.
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
