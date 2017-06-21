/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");

var fxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

// for master-password utilities
Cu.import("resource://services-sync/util.js");

const PREF_LAST_FXA_USER = "identity.fxaccounts.lastSignedInUserHash";

const ACTION_URL_PARAM = "action";

const OBSERVER_TOPICS = [
  fxAccountsCommon.ONVERIFIED_NOTIFICATION,
  fxAccountsCommon.ONLOGOUT_NOTIFICATION,
];

function log(msg) {
  // dump("FXA: " + msg + "\n");
}

function getPreviousAccountNameHash() {
  try {
    return Services.prefs.getStringPref(PREF_LAST_FXA_USER);
  } catch (_) {
    return "";
  }
}

function setPreviousAccountNameHash(acctName) {
  Services.prefs.setStringPref(PREF_LAST_FXA_USER, sha256(acctName));
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

function updateDisplayedEmail(user) {
  let emailDiv = document.getElementById("email");
  if (emailDiv && user) {
    emailDiv.textContent = user.email;
  }
}

var wrapper = {
  iframe: null,

  init(url, urlParams) {
    // If a master-password is enabled, we want to encourage the user to
    // unlock it.  Things still work if not, but the user will probably need
    // to re-auth next startup (in which case we will get here again and
    // re-prompt)
    Utils.ensureMPUnlocked();

    let iframe = document.getElementById("remote");
    this.iframe = iframe;
    this.iframe.QueryInterface(Ci.nsIFrameLoaderOwner);
    let docShell = this.iframe.frameLoader.docShell;
    docShell.QueryInterface(Ci.nsIWebProgress);
    docShell.addProgressListener(this.iframeListener,
                                 Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
                                 Ci.nsIWebProgress.NOTIFY_LOCATION);
    iframe.addEventListener("load", this);

    // Ideally we'd just merge urlParams with new URL(url).searchParams, but our
    // URLSearchParams implementation doesn't support iteration (bug 1085284).
    let urlParamStr = urlParams.toString();
    if (urlParamStr) {
      url += (url.includes("?") ? "&" : "?") + urlParamStr;
    }
    this.url = url;
    // Set the iframe's location with loadURI/LOAD_FLAGS_REPLACE_HISTORY to
    // avoid having a new history entry being added. REPLACE_HISTORY is used
    // to replace the current entry, which is `about:blank`.
    let webNav = iframe.frameLoader.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.loadURI(url, Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY, null, null, null);
  },

  retry() {
    let webNav = this.iframe.frameLoader.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.loadURI(this.url, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY, null, null, null);
  },

  iframeListener: {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

    onStateChange(aWebProgress, aRequest, aState, aStatus) {
      let failure = false;

      // Captive portals sometimes redirect users
      if ((aState & Ci.nsIWebProgressListener.STATE_REDIRECTING)) {
        failure = true;
      } else if ((aState & Ci.nsIWebProgressListener.STATE_STOP)) {
        if (aRequest instanceof Ci.nsIHttpChannel) {
          try {
            failure = aRequest.responseStatus != 200;
          } catch (e) {
            failure = aStatus != Components.results.NS_OK;
          }
        }
      }

      // Calling cancel() will raise some OnStateChange notifications by itself,
      // so avoid doing that more than once
      if (failure && aStatus != Components.results.NS_BINDING_ABORTED) {
        aRequest.cancel(Components.results.NS_BINDING_ABORTED);
        setErrorPage("networkError");
      }
    },

    onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
      if (aRequest && aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
        aRequest.cancel(Components.results.NS_BINDING_ABORTED);
        setErrorPage("networkError");
      }
    },
  },

  handleEvent(evt) {
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
  onLogin(accountData) {
    log("Received: 'login'. Data:" + JSON.stringify(accountData));

    // We don't act on customizeSync anymore, it used to open a dialog inside
    // the browser to selecte the engines to sync but we do it on the web now.
    delete accountData.customizeSync;
    // sessionTokenContext is erroneously sent by the content server.
    // https://github.com/mozilla/fxa-content-server/issues/2766
    // To avoid having the FxA storage manager not knowing what to do with
    // it we delete it here.
    delete accountData.sessionTokenContext;

    // We need to confirm a relink - see shouldAllowRelink for more
    let newAccountEmail = accountData.email;
    // The hosted code may have already checked for the relink situation
    // by sending the can_link_account command. If it did, then
    // it will indicate we don't need to ask twice.
    if (!accountData.verifiedCanLinkAccount && !shouldAllowRelink(newAccountEmail)) {
      // we need to tell the page we successfully received the message, but
      // then bail without telling fxAccounts
      this.injectData("message", { status: "login" });
      // after a successful login we return to preferences
      openPrefs();
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
      updateDisplayedEmail(accountData);
      return fxAccounts.setSignedInUser(accountData);
    }).then(() => {
      // If the user data is verified, we want it to immediately look like
      // they are signed in without waiting for messages to bounce around.
      if (accountData.verified) {
        openPrefs();
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

  onCanLinkAccount(accountData) {
    // We need to confirm a relink - see shouldAllowRelink for more
    let ok = shouldAllowRelink(accountData.email);
    this.injectData("message", { status: "can_link_account", data: { ok } });
  },

  /**
   * onSignOut handler erases the current user's session from the fxaccounts service
   */
  onSignOut() {
    log("Received: 'sign_out'.");

    fxAccounts.signOut().then(
      () => this.injectData("message", { status: "sign_out" }),
      (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  handleRemoteCommand(evt) {
    log("command: " + evt.detail.command);
    let data = evt.detail.data;

    switch (evt.detail.command) {
      case "login":
        this.onLogin(data);
        break;
      case "can_link_account":
        this.onCanLinkAccount(data);
        break;
      case "sign_out":
        this.onSignOut(data);
        break;
      default:
        log("Unexpected remote command received: " + evt.detail.command + ". Ignoring command.");
        break;
    }
  },

  injectData(type, content) {
    return fxAccounts.promiseAccountsSignUpURI().then(authUrl => {
      let data = {
        type,
        content
      };
      this.iframe.contentWindow.postMessage(data, authUrl);
    })
    .catch(e => {
      console.log("Failed to inject data", e);
      setErrorPage("configError");
    });
  },
};


// Button onclick handlers

function getStarted() {
  show("remote");
}

function retry() {
  show("remote");
  wrapper.retry();
}

function openPrefs() {
  // Bug 1199303 calls for this tab to always be replaced with Preferences
  // rather than it opening in a different tab.
  window.location = "about:preferences#sync";
}

function init() {
  fxAccounts.getSignedInUser().then(user => {
    // tests in particular might cause the window to start closing before
    // getSignedInUser has returned.
    if (window.closed) {
      return Promise.resolve();
    }

    updateDisplayedEmail(user);

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
        return fxAccounts.promiseAccountsSignInURI().then(url => {
          show("remote");
          wrapper.init(url, urlParams);
        });
      }
      break;
    case "signup":
      if (user) {
        // asking to sign-up when already signed in just shows manage.
        show("stage", "manage");
      } else {
        return fxAccounts.promiseAccountsSignUpURI().then(url => {
          show("remote");
          wrapper.init(url, urlParams);
        });
      }
      break;
    case "reauth":
      // ideally we would only show this when we know the user is in a
      // "must reauthenticate" state - but we don't.
      // As the email address will be included in the URL returned from
      // promiseAccountsForceSigninURI, just always show it.
      return fxAccounts.promiseAccountsForceSigninURI().then(url => {
        show("remote");
        wrapper.init(url, urlParams);
      });
    default:
      // No action specified.
      if (user) {
        show("stage", "manage");
      } else {
        // Attempt a migration if enabled or show the introductory page
        // otherwise.
        return migrateToDevEdition(urlParams).then(migrated => {
          if (!migrated) {
            show("stage", "intro");
            // load the remote frame in the background
            return fxAccounts.promiseAccountsSignUpURI().then(uri =>
              wrapper.init(uri, urlParams));
          }
          return Promise.resolve();
        });
      }
      break;
    }
    return Promise.resolve();
  }).catch(err => {
    console.log("Configuration or sign in error", err);
    setErrorPage("configError");
  });
}

function setErrorPage(errorType) {
  show("stage", errorType);
}

// Causes the "top-level" element with |id| to be shown - all other top-level
// elements are hidden.  Optionally, ensures that only 1 "second-level" element
// inside the top-level one is shown.
function show(id, childId) {
  // top-level items are either <div> or <iframe>
  let allTop = document.querySelectorAll("body > div, iframe");
  for (let elt of allTop) {
    if (elt.getAttribute("id") == id) {
      elt.style.display = "block";
    } else {
      elt.style.display = "none";
    }
  }
  if (childId) {
    // child items are all <div>
    let allSecond = document.querySelectorAll("#" + id + " > div");
    for (let elt of allSecond) {
      if (elt.getAttribute("id") == childId) {
        elt.style.display = "block";
      } else {
        elt.style.display = "none";
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

  if (!defaultProfilePath ||
      !Services.prefs.getBoolPref("identity.fxaccounts.migrateToDevEdition", false)) {
    return Promise.resolve(false);
  }

  Cu.import("resource://gre/modules/osfile.jsm");
  let fxAccountsStorage = OS.Path.join(defaultProfilePath, fxAccountsCommon.DEFAULT_STORAGE_FILENAME);
  return OS.File.read(fxAccountsStorage, { encoding: "utf-8" }).then(text => {
    let accountData = JSON.parse(text).accountData;
    updateDisplayedEmail(accountData);
    return fxAccounts.setSignedInUser(accountData);
  }).then(() => {
    return fxAccounts.promiseAccountsForceSigninURI().then(url => {
      show("remote");
      wrapper.init(url, urlParams);
    });
  }).catch(error => {
    log("Failed to migrate FX Account: " + error);
    show("stage", "intro");
    // load the remote frame in the background
    fxAccounts.promiseAccountsSignUpURI().then(uri => {
      wrapper.init(uri, urlParams)
    }).catch(e => {
      console.log("Failed to load signup page", e);
      setErrorPage("configError");
    });
  }).then(() => {
    // Reset the pref after migration.
    Services.prefs.setBoolPref("identity.fxaccounts.migrateToDevEdition", false);
    return true;
  }).catch(err => {
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

document.addEventListener("DOMContentLoaded", function() {
  init();
  var buttonGetStarted = document.getElementById("buttonGetStarted");
  buttonGetStarted.addEventListener("click", getStarted);

  var buttonRetry = document.getElementById("buttonRetry");
  buttonRetry.addEventListener("click", retry);

  var buttonOpenPrefs = document.getElementById("buttonOpenPrefs");
  buttonOpenPrefs.addEventListener("click", openPrefs);
}, {capture: true, once: true});

function initObservers() {
  function observe(subject, topic, data) {
    log("about:accounts observed " + topic);
    if (topic == fxAccountsCommon.ONLOGOUT_NOTIFICATION) {
      // All about:account windows get changed to action=signin on logout.
      window.location = "about:accounts?action=signin";
      return;
    }

    // must be onverified - we want to open preferences.
    openPrefs();
  }

  for (let topic of OBSERVER_TOPICS) {
    Services.obs.addObserver(observe, topic);
  }
  window.addEventListener("unload", function(event) {
    log("about:accounts unloading")
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(observe, topic);
    }
  });
}
initObservers();
