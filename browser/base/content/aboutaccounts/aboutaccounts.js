/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");

const PREF_LAST_FXA_USER = "identity.fxaccounts.lastSignedInUserHash";
const PREF_SYNC_SHOW_CUSTOMIZATION = "services.sync.ui.showCustomizationDialog";

function log(msg) {
  //dump("FXA: " + msg + "\n");
};

function error(msg) {
  console.log("Firefox Account Error: " + msg + "\n");
};

function getPreviousAccountNameHash() {
  try {
    return Services.prefs.getComplexValue(PREF_LAST_FXA_USER, Ci.nsISupportsString).data;
  } catch (_) {
    return "";
  }
}

function setPreviousAccountNameHash(acctName) {
  let string = Cc["@mozilla.org/supports-string;1"]
               .createInstance(Ci.nsISupportsString);
  string.data = sha256(acctName);
  Services.prefs.setComplexValue(PREF_LAST_FXA_USER, Ci.nsISupportsString, string);
}

function needRelinkWarning(acctName) {
  let prevAcctHash = getPreviousAccountNameHash();
  return prevAcctHash && prevAcctHash != sha256(acctName);
}

// Given a string, returns the SHA265 hash in base64
function sha256(str) {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  // Data is an array of bytes.
  let data = converter.convertToByteArray(str, {});
  let hasher = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  return hasher.finish(true);
}

function promptForRelink(acctName) {
  let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
  let continueLabel = sb.GetStringFromName("continue.label");
  let title = sb.GetStringFromName("relink.verify.title");
  let description = sb.formatStringFromName("relink.verify.description",
                                            [acctName], 1);
  let body = sb.GetStringFromName("relink.verify.heading") +
             "\n\n" + description;
  let ps = Services.prompt;
  let buttonFlags = (ps.BUTTON_POS_0 * ps.BUTTON_TITLE_IS_STRING) +
                    (ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL) +
                    ps.BUTTON_POS_1_DEFAULT;
  let pressed = Services.prompt.confirmEx(window, title, body, buttonFlags,
                                     continueLabel, null, null, null,
                                     {});
  return pressed == 0; // 0 is the "continue" button
}

let wrapper = {
  iframe: null,

  init: function () {
    let weave = Cc["@mozilla.org/weave/service;1"]
                  .getService(Ci.nsISupports)
                  .wrappedJSObject;

    // Don't show about:accounts with FxA disabled.
    if (!weave.fxAccountsEnabled) {
      document.body.remove();
      return;
    }

    let iframe = document.getElementById("remote");
    this.iframe = iframe;
    iframe.addEventListener("load", this);

    try {
      iframe.src = fxAccounts.getAccountsURI();
    } catch (e) {
      error("Couldn't init Firefox Account wrapper: " + e.message);
    }
  },

  handleEvent: function (evt) {
    switch (evt.type) {
      case "load":
        this.iframe.contentWindow.addEventListener("FirefoxAccountsCommand", this);
        this.iframe.removeEventListener("load", this);
        break;
      case "FirefoxAccountsCommand":
        this.handleRemoteCommand(evt);
        break;
    }
  },

  /**
   * onLogin handler receives user credentials from the jelly after a
   * sucessful login and stores it in the fxaccounts service
   *
   * @param accountData the user's account data and credentials
   */
  onLogin: function (accountData) {
    log("Received: 'login'. Data:" + JSON.stringify(accountData));

    if (accountData.customizeSync) {
      Services.prefs.setBoolPref(PREF_SYNC_SHOW_CUSTOMIZATION, true);
      delete accountData.customizeSync;
    }

    // If the last fxa account used for sync isn't this account, we display
    // a modal dialog checking they really really want to do this...
    // (This is sync-specific, so ideally would be in sync's identity module,
    // but it's a little more seamless to do here, and sync is currently the
    // only fxa consumer, so...
    let newAccountEmail = accountData.email;
    if (needRelinkWarning(newAccountEmail) && !promptForRelink(newAccountEmail)) {
      // we need to tell the page we successfully received the message, but
      // then bail without telling fxAccounts
      this.injectData("message", { status: "login" });
      // and reload the page or else it remains in a "signed in" state.
      window.location.reload();
      return;
    }

    // Remember who it was so we can log out next time.
    setPreviousAccountNameHash(newAccountEmail);

    fxAccounts.setSignedInUser(accountData).then(
      () => {
        this.injectData("message", { status: "login" });
        // until we sort out a better UX, just leave the jelly page in place.
        // If the account email is not yet verified, it will tell the user to
        // go check their email, but then it will *not* change state after
        // the verification completes (the browser will begin syncing, but
        // won't notify the user). If the email has already been verified,
        // the jelly will say "Welcome! You are successfully signed in as
        // EMAIL", but it won't then say "syncing started".
      },
      (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  /**
   * onSessionStatus sends the currently signed in user's credentials
   * to the jelly.
   */
  onSessionStatus: function () {
    log("Received: 'session_status'.");

    fxAccounts.getSignedInUser().then(
      (accountData) => this.injectData("message", { status: "session_status", data: accountData }),
      (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  /**
   * onSignOut handler erases the current user's session from the fxaccounts service
   */
  onSignOut: function () {
    log("Received: 'sign_out'.");

    fxAccounts.signOut().then(
      () => this.injectData("message", { status: "sign_out" }),
      (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  handleRemoteCommand: function (evt) {
    log('command: ' + evt.detail.command);
    let data = evt.detail.data;

    switch (evt.detail.command) {
      case "login":
        this.onLogin(data);
        break;
      case "session_status":
        this.onSessionStatus(data);
        break;
      case "sign_out":
        this.onSignOut(data);
        break;
      default:
        log("Unexpected remote command received: " + evt.detail.command + ". Ignoring command.");
        break;
    }
  },

  injectData: function (type, content) {
    let authUrl;
    try {
      authUrl = fxAccounts.getAccountsURI();
    } catch (e) {
      error("Couldn't inject data: " + e.message);
      return;
    }
    let data = {
      type: type,
      content: content
    };
    this.iframe.contentWindow.postMessage(data, authUrl);
  },
};


// Button onclick handlers
function handleOldSync() {
  // we just want to navigate the current tab to the new location...
  window.location = Services.urlFormatter.formatURLPref("app.support.baseURL") + "old-sync";
}

function getStarted() {
  hide("intro");
  hide("stage");
  show("remote");
}

function openPrefs() {
  window.openPreferences("paneSync");
}

function init() {
  let signinQuery = window.location.href.match(/signin=true$/);

  if (signinQuery) {
    show("remote");
    wrapper.init();
  } else {
    // Check if we have a local account
    fxAccounts.getSignedInUser().then(user => {
      if (user) {
        show("stage");
        show("manage");
        let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
        document.title = sb.GetStringFromName("manage.pageTitle");
      } else {
        show("stage");
        show("intro");
        // load the remote frame in the background
        wrapper.init();
      }
    });
  }
}

function show(id) {
  document.getElementById(id).style.display = 'block';
}
function hide(id) {
  document.getElementById(id).style.display = 'none';
}

document.addEventListener("DOMContentLoaded", function onload() {
  document.removeEventListener("DOMContentLoaded", onload, true);
  init();
}, true);
