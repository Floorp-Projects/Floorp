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

const ACTION_URL_PARAM = "action";

function log(msg) {
  // dump("FXA: " + msg + "\n");
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
    let docShell = this.iframe.frameLoader.docShell;
    docShell.QueryInterface(Ci.nsIWebProgress);
    docShell.addProgressListener(this.iframeListener,
                                 Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
                                 Ci.nsIWebProgress.NOTIFY_LOCATION);

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
  }
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
      wrapper.init(uri, urlParams);
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
    window.location = "about:accounts?action=signin";
  }

  Services.obs.addObserver(observe, fxAccountsCommon.ONLOGOUT_NOTIFICATION);

  window.addEventListener("unload", function(event) {
    log("about:accounts unloading");
    Services.obs.removeObserver(observe,
                                fxAccountsCommon.ONLOGOUT_NOTIFICATION);
  });
}
initObservers();
