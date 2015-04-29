/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");

let fxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

// for master-password utilities
Cu.import("resource://services-sync/util.js");

const PREF_LAST_FXA_USER = "identity.fxaccounts.lastSignedInUserHash";
const PREF_SYNC_SHOW_CUSTOMIZATION = "services.sync-setup.ui.showCustomizationDialog";

const ACTION_URL_PARAM = "action";

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

  init: function (url, urlParams) {
    // If a master-password is enabled, we want to encourage the user to
    // unlock it.  Things still work if not, but the user will probably need
    // to re-auth next startup (in which case we will get here again and
    // re-prompt)
    Utils.ensureMPUnlocked();

    let iframe = document.getElementById("remote");
    this.iframe = iframe;
    iframe.addEventListener("load", this);

    // Ideally we'd just merge urlParams with new URL(url).searchParams, but our
    // URLSearchParams implementation doesn't support iteration (bug 1085284).
    let urlParamStr = urlParams.toString();
    if (urlParamStr) {
      url += (url.includes("?") ? "&" : "?") + urlParamStr;
    }
    iframe.src = url;
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
        show("stage", "manage");
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

    // Ideally we'd use new URL(document.URL).searchParams, but for about: URIs,
    // searchParams is empty.
    let urlParams = new URLSearchParams(document.URL.split("?")[1] || "");
    let action = urlParams.get(ACTION_URL_PARAM);
    urlParams.delete(ACTION_URL_PARAM);

    switch (action) {
    case "signin":
      if (user) {
        // asking to sign-in when already signed in just shows manage.
        show("stage", "manage");
      } else {
        show("remote");
        wrapper.init(fxAccounts.getAccountsSignInURI(), urlParams);
      }
      break;
    case "signup":
      if (user) {
        // asking to sign-up when already signed in just shows manage.
        show("stage", "manage");
      } else {
        show("remote");
        wrapper.init(fxAccounts.getAccountsSignUpURI(), urlParams);
      }
      break;
    case "reauth":
      // ideally we would only show this when we know the user is in a
      // "must reauthenticate" state - but we don't.
      // As the email address will be included in the URL returned from
      // promiseAccountsForceSigninURI, just always show it.
      fxAccounts.promiseAccountsForceSigninURI().then(url => {
        show("remote");
        wrapper.init(url, urlParams);
      });
      break;
    default:
      // No action specified.
      if (user) {
        show("stage", "manage");
        let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
        document.title = sb.GetStringFromName("manage.pageTitle");
      } else {
        // Attempt a migration if enabled or show the introductory page
        // otherwise.
        migrateToDevEdition(urlParams).then(migrated => {
          if (!migrated) {
            show("stage", "intro");
            // load the remote frame in the background
            wrapper.init(fxAccounts.getAccountsSignUpURI(), urlParams);
          }
        });
      }
      break;
    }
  });
}

// Causes the "top-level" element with |id| to be shown - all other top-level
// elements are hidden.  Optionally, ensures that only 1 "second-level" element
// inside the top-level one is shown.
function show(id, childId) {
  // top-level items are either <div> or <iframe>
  let allTop = document.querySelectorAll("body > div, iframe");
  for (let elt of allTop) {
    if (elt.getAttribute("id") == id) {
      elt.style.display = 'block';
    } else {
      elt.style.display = 'none';
    }
  }
  if (childId) {
    // child items are all <div>
    let allSecond = document.querySelectorAll("#" + id + " > div");
    for (let elt of allSecond) {
      if (elt.getAttribute("id") == childId) {
        elt.style.display = 'block';
      } else {
        elt.style.display = 'none';
      }
    }
  }
}

// Migrate sync data from the default profile to the dev-edition profile.
// Returns a promise of a true value if migration succeeded, or false if it
// failed.
function migrateToDevEdition(urlParams) {
  let defaultProfilePath;
  try {
    defaultProfilePath = window.getDefaultProfilePath();
  } catch (e) {} // no default profile.
  let migrateSyncCreds = false;
  if (defaultProfilePath) {
    try {
      migrateSyncCreds = Services.prefs.getBoolPref("identity.fxaccounts.migrateToDevEdition");
    } catch (e) {}
  }

  if (!migrateSyncCreds) {
    return Promise.resolve(false);
  }

  Cu.import("resource://gre/modules/osfile.jsm");
  let fxAccountsStorage = OS.Path.join(defaultProfilePath, fxAccountsCommon.DEFAULT_STORAGE_FILENAME);
  return OS.File.read(fxAccountsStorage, { encoding: "utf-8" }).then(text => {
    let accountData = JSON.parse(text).accountData;
    return fxAccounts.setSignedInUser(accountData);
  }).then(() => {
    return fxAccounts.promiseAccountsForceSigninURI().then(url => {
      show("remote");
      wrapper.init(url, urlParams);
    });
  }).then(null, error => {
    log("Failed to migrate FX Account: " + error);
    show("stage", "intro");
    // load the remote frame in the background
    wrapper.init(fxAccounts.getAccountsSignUpURI(), urlParams);
  }).then(() => {
    // Reset the pref after migration.
    Services.prefs.setBoolPref("identity.fxaccounts.migrateToDevEdition", false);
    return true;
  }).then(null, err => {
    Cu.reportError("Failed to reset the migrateToDevEdition pref: " + err);
    return false;
  });
}

// Helper function that returns the path of the default profile on disk. Will be
// overridden in tests.
function getDefaultProfilePath() {
  let defaultProfile = Cc["@mozilla.org/toolkit/profile-service;1"]
                        .getService(Ci.nsIToolkitProfileService)
                        .defaultProfile;
  return defaultProfile.rootDir.path;
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
