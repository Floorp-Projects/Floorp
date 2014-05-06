/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");

let fxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

const PREF_LAST_FXA_USER = "identity.fxaccounts.lastSignedInUserHash";
const PREF_SYNC_SHOW_CUSTOMIZATION = "services.sync.ui.showCustomizationDialog";

const OBSERVER_TOPICS = [
  fxAccountsCommon.ONVERIFIED_NOTIFICATION,
  fxAccountsCommon.ONLOGOUT_NOTIFICATION,
];

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
  let title = sb.GetStringFromName("relinkVerify.title");
  let description = sb.formatStringFromName("relinkVerify.description",
                                            [acctName], 1);
  let body = sb.GetStringFromName("relinkVerify.heading") +
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

// If the last fxa account used for sync isn't this account, we display
// a modal dialog checking they really really want to do this...
// (This is sync-specific, so ideally would be in sync's identity module,
// but it's a little more seamless to do here, and sync is currently the
// only fxa consumer, so...
function shouldAllowRelink(acctName) {
  return !needRelinkWarning(acctName) || promptForRelink(acctName);
}

let wrapper = {
  iframe: null,

  init: function (url=null) {
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
      iframe.src = url || fxAccounts.getAccountsSignUpURI();
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

    // We need to confirm a relink - see shouldAllowRelink for more
    let newAccountEmail = accountData.email;
    // The hosted code may have already checked for the relink situation
    // by sending the can_link_account command. If it did, then
    // it will indicate we don't need to ask twice.
    if (!accountData.verifiedCanLinkAccount && !shouldAllowRelink(newAccountEmail)) {
      // we need to tell the page we successfully received the message, but
      // then bail without telling fxAccounts
      this.injectData("message", { status: "login" });
      // and re-init the page by navigating to about:accounts
      window.location = "about:accounts";
      return;
    }
    delete accountData.verifiedCanLinkAccount;

    // Remember who it was so we can log out next time.
    setPreviousAccountNameHash(newAccountEmail);

    // A sync-specific hack - we want to ensure sync has been initialized
    // before we set the signed-in user.
    let xps = Cc["@mozilla.org/weave/service;1"]
              .getService(Ci.nsISupports)
              .wrappedJSObject;
    xps.whenLoaded().then(() => {
      return fxAccounts.setSignedInUser(accountData);
    }).then(() => {
      // If the user data is verified, we want it to immediately look like
      // they are signed in without waiting for messages to bounce around.
      if (accountData.verified) {
        showManage();
      }
      this.injectData("message", { status: "login" });
      // until we sort out a better UX, just leave the jelly page in place.
      // If the account email is not yet verified, it will tell the user to
      // go check their email, but then it will *not* change state after
      // the verification completes (the browser will begin syncing, but
      // won't notify the user). If the email has already been verified,
      // the jelly will say "Welcome! You are successfully signed in as
      // EMAIL", but it won't then say "syncing started".
    }, (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  onCanLinkAccount: function(accountData) {
    // We need to confirm a relink - see shouldAllowRelink for more
    let ok = shouldAllowRelink(accountData.email);
    this.injectData("message", { status: "can_link_account", data: { ok: ok } });
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
      case "can_link_account":
        this.onCanLinkAccount(data);
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
      authUrl = fxAccounts.getAccountsSignUpURI();
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
  let chromeWin = window
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow);
  let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "old-sync";
  chromeWin.switchToTabHavingURI(url, true);
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
  fxAccounts.getSignedInUser().then(user => {
    // tests in particular might cause the window to start closing before
    // getSignedInUser has returned.
    if (window.closed) {
      return;
    }
    if (window.location.href.contains("action=signin")) {
      if (user) {
        // asking to sign-in when already signed in just shows manage.
        showManage();
      } else {
        show("remote");
        wrapper.init(fxAccounts.getAccountsSignInURI());
      }
    } else if (window.location.href.contains("action=signup")) {
      if (user) {
        // asking to sign-up when already signed in just shows manage.
        showManage();
      } else {
        show("remote");
        wrapper.init();
      }
    } else if (window.location.href.contains("action=reauth")) {
      // ideally we would only show this when we know the user is in a
      // "must reauthenticate" state - but we don't.
      // As the email address will be included in the URL returned from
      // promiseAccountsForceSigninURI, just always show it.
      fxAccounts.promiseAccountsForceSigninURI().then(url => {
        show("remote");
        wrapper.init(url);
      });
    } else {
      // No action specified
      if (user) {
        showManage();
        let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
        document.title = sb.GetStringFromName("manage.pageTitle");
      } else {
        show("stage");
        show("intro");
        // load the remote frame in the background
        wrapper.init();
      }
    }
  });
}

function show(id) {
  document.getElementById(id).style.display = 'block';
}
function hide(id) {
  document.getElementById(id).style.display = 'none';
}

function showManage() {
  show("stage");
  show("manage");
  hide("remote");
  hide("intro");
}

document.addEventListener("DOMContentLoaded", function onload() {
  document.removeEventListener("DOMContentLoaded", onload, true);
  init();
  var buttonGetStarted = document.getElementById('buttonGetStarted');
  buttonGetStarted.addEventListener('click', getStarted);

  var oldsync = document.getElementById('oldsync');
  oldsync.addEventListener('click', handleOldSync);

  var buttonOpenPrefs = document.getElementById('buttonOpenPrefs')
  buttonOpenPrefs.addEventListener('click', openPrefs);
}, true);

function initObservers() {
  function observe(subject, topic, data) {
    log("about:accounts observed " + topic);
    if (topic == fxAccountsCommon.ONLOGOUT_NOTIFICATION) {
      // All about:account windows get changed to action=signin on logout.
      window.location = "about:accounts?action=signin";
      return;
    }
    // must be onverified - just about:accounts is loaded.
    window.location = "about:accounts";
  }

  for (let topic of OBSERVER_TOPICS) {
    Services.obs.addObserver(observe, topic, false);
  }
  window.addEventListener("unload", function(event) {
    log("about:accounts unloading")
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(observe, topic);
    }
  });
}
initObservers();
