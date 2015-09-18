/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://services-sync/main.js");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function () {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "fxaMigrator",
  "resource://services-sync/FxaMigrator.jsm");

const PAGE_NO_ACCOUNT = 0;
const PAGE_HAS_ACCOUNT = 1;
const PAGE_NEEDS_UPDATE = 2;
const FXA_PAGE_LOGGED_OUT = 3;
const FXA_PAGE_LOGGED_IN = 4;

// Indexes into the "login status" deck.
// We are in a successful verified state - everything should work!
const FXA_LOGIN_VERIFIED = 0;
// We have logged in to an unverified account.
const FXA_LOGIN_UNVERIFIED = 1;
// We are logged in locally, but the server rejected our credentials.
const FXA_LOGIN_FAILED = 2;

var gSyncPane = {
  prefArray: ["engine.bookmarks", "engine.passwords", "engine.prefs",
              "engine.tabs", "engine.history"],

  get page() {
    return document.getElementById("weavePrefsDeck").selectedIndex;
  },

  set page(val) {
    document.getElementById("weavePrefsDeck").selectedIndex = val;
  },

  get _usingCustomServer() {
    return Weave.Svc.Prefs.isSet("serverURL");
  },

  needsUpdate: function () {
    this.page = PAGE_NEEDS_UPDATE;
    let label = document.getElementById("loginError");
    label.textContent = Weave.Utils.getErrorString(Weave.Status.login);
    label.className = "error";
  },

  init: function () {
    this._setupEventListeners();

    // If the Service hasn't finished initializing, wait for it.
    let xps = Components.classes["@mozilla.org/weave/service;1"]
                                .getService(Components.interfaces.nsISupports)
                                .wrappedJSObject;

    if (xps.ready) {
      this._init();
      return;
    }

    // it may take some time before we can determine what provider to use
    // and the state of that provider, so show the "please wait" page.
    this._showLoadPage(xps);

    let onUnload = function () {
      window.removeEventListener("unload", onUnload, false);
      try {
        Services.obs.removeObserver(onReady, "weave:service:ready");
      } catch (e) {}
    };

    let onReady = function () {
      Services.obs.removeObserver(onReady, "weave:service:ready");
      window.removeEventListener("unload", onUnload, false);
      this._init();
    }.bind(this);

    Services.obs.addObserver(onReady, "weave:service:ready", false);
    window.addEventListener("unload", onUnload, false);

    xps.ensureLoaded();
  },

  _showLoadPage: function (xps) {
    let username;
    try {
      username = Services.prefs.getCharPref("services.sync.username");
    } catch (e) {}
    if (!username) {
      this.page = FXA_PAGE_LOGGED_OUT;
    } else if (xps.fxAccountsEnabled) {
      // Use cached values while we wait for the up-to-date values
      let cachedComputerName;
      try {
        cachedComputerName = Services.prefs.getCharPref("services.sync.client.name");
      }
      catch (e) {
        cachedComputerName = "";
      }
      document.getElementById("fxaEmailAddress1").textContent = username;
      document.getElementById("fxaSyncComputerName").value = cachedComputerName;
      this.page = FXA_PAGE_LOGGED_IN;
    } else { // Old Sync
      this.page = PAGE_HAS_ACCOUNT;
    }
  },

  _init: function () {
    let topics = ["weave:service:login:error",
                  "weave:service:login:finish",
                  "weave:service:start-over:finish",
                  "weave:service:setup-complete",
                  "weave:service:logout:finish",
                  FxAccountsCommon.ONVERIFIED_NOTIFICATION,
                  FxAccountsCommon.ONLOGIN_NOTIFICATION,
                  FxAccountsCommon.ON_PROFILE_CHANGE_NOTIFICATION,
                  ];
    let migrateTopic = "fxa-migration:state-changed";

    // Add the observers now and remove them on unload
    //XXXzpao This should use Services.obs.* but Weave's Obs does nice handling
    //        of `this`. Fix in a followup. (bug 583347)
    topics.forEach(function (topic) {
      Weave.Svc.Obs.add(topic, this.updateWeavePrefs, this);
    }, this);
    // The FxA migration observer is a special case.
    Weave.Svc.Obs.add(migrateTopic, this.updateMigrationState, this);

    window.addEventListener("unload", function() {
      topics.forEach(function (topic) {
        Weave.Svc.Obs.remove(topic, this.updateWeavePrefs, this);
      }, gSyncPane);
      Weave.Svc.Obs.remove(migrateTopic, gSyncPane.updateMigrationState, gSyncPane);
    }, false);

    XPCOMUtils.defineLazyGetter(this, '_stringBundle', () => {
      return Services.strings.createBundle("chrome://browser/locale/preferences/preferences.properties");
    }),

    XPCOMUtils.defineLazyGetter(this, '_accountsStringBundle', () => {
      return Services.strings.createBundle("chrome://browser/locale/accounts.properties");
    }),

    this.updateWeavePrefs();

    this._initProfileImageUI();
  },

  _toggleComputerNameControls: function(editMode) {
    let textbox = document.getElementById("fxaSyncComputerName");
    textbox.disabled = !editMode;
    document.getElementById("fxaChangeDeviceName").hidden = editMode;
    document.getElementById("fxaCancelChangeDeviceName").hidden = !editMode;
    document.getElementById("fxaSaveChangeDeviceName").hidden = !editMode;
  },

  _focusComputerNameTextbox: function() {
    let textbox = document.getElementById("fxaSyncComputerName");
    let valLength = textbox.value.length;
    textbox.focus();
    textbox.setSelectionRange(valLength, valLength);
  },

  _blurComputerNameTextbox: function() {
    document.getElementById("fxaSyncComputerName").blur();
  },

  _focusChangeDeviceNameButton: function() {
    document.getElementById("fxaChangeDeviceName").focus();
  },

  _updateComputerNameValue: function(save) {
    let textbox = document.getElementById("fxaSyncComputerName");
    if (save) {
      Weave.Service.clientsEngine.localName = textbox.value;
    }
    else {
      textbox.value = Weave.Service.clientsEngine.localName;
    }
  },

  _closeSyncStatusMessageBox: function() {
    document.getElementById("syncStatusMessage").removeAttribute("message-type");
    document.getElementById("syncStatusMessageTitle").textContent = "";
    document.getElementById("syncStatusMessageDescription").textContent = "";
    let learnMoreLink = document.getElementById("learnMoreLink");
    if (learnMoreLink) {
      learnMoreLink.parentNode.removeChild(learnMoreLink);
    }
    document.getElementById("sync-migration-buttons-deck").hidden = true;
  },

  _setupEventListeners: function() {
    function setEventListener(aId, aEventType, aCallback)
    {
      document.getElementById(aId)
              .addEventListener(aEventType, aCallback.bind(gSyncPane));
    }

    setEventListener("syncStatusMessageClose", "command", function () {
      gSyncPane._closeSyncStatusMessageBox();
    });
    setEventListener("noAccountSetup", "click", function (aEvent) {
      aEvent.stopPropagation();
      gSyncPane.openSetup(null);
    });
    setEventListener("noAccountPair", "click", function (aEvent) {
      aEvent.stopPropagation();
      gSyncPane.openSetup('pair');
    });
    setEventListener("syncChangePassword", "command",
      () => gSyncUtils.changePassword());
    setEventListener("syncResetPassphrase", "command",
      () => gSyncUtils.resetPassphrase());
    setEventListener("syncReset", "command", gSyncPane.resetSync);
    setEventListener("syncAddDeviceLabel", "click", function () {
      gSyncPane.openAddDevice();
      return false;
    });
    setEventListener("syncEnginesList", "select", function () {
      if (this.selectedCount)
        this.clearSelection();
    });
    setEventListener("syncComputerName", "change", function (e) {
      gSyncUtils.changeName(e.target);
    });
    setEventListener("fxaChangeDeviceName", "command", function () {
      this._toggleComputerNameControls(true);
      this._focusComputerNameTextbox();
    });
    setEventListener("fxaCancelChangeDeviceName", "command", function () {
      // We explicitly blur the textbox because of bug 1194032
      this._blurComputerNameTextbox();
      this._toggleComputerNameControls(false);
      this._updateComputerNameValue(false);
      this._focusChangeDeviceNameButton();
    });
    setEventListener("fxaSaveChangeDeviceName", "command", function () {
      this._blurComputerNameTextbox();
      this._toggleComputerNameControls(false);
      this._updateComputerNameValue(true);
      this._focusChangeDeviceNameButton();
    });
    setEventListener("unlinkDevice", "click", function () {
      gSyncPane.startOver(true);
      return false;
    });
    setEventListener("tosPP-normal-ToS", "click", gSyncPane.openToS);
    setEventListener("tosPP-normal-PP", "click", gSyncPane.openPrivacyPolicy);
    setEventListener("loginErrorUpdatePass", "click", function () {
      gSyncPane.updatePass();
      return false;
    });
    setEventListener("loginErrorResetPass", "click", function () {
      gSyncPane.resetPass();
      return false;
    });
    setEventListener("loginErrorStartOver", "click", function () {
      gSyncPane.startOver(true);
      return false;
    });
    setEventListener("noFxaSignUp", "command", function () {
      gSyncPane.signUp();
      return false;
    });
    setEventListener("noFxaSignIn", "command", function () {
      gSyncPane.signIn();
      return false;
    });
    setEventListener("verifiedManage", "click",
      gSyncPane.manageFirefoxAccount);
    setEventListener("fxaUnlinkButton", "click", function () {
      gSyncPane.unlinkFirefoxAccount(true);
    });
    setEventListener("verifyFxaAccount", "command",
      gSyncPane.verifyFirefoxAccount);
    setEventListener("unverifiedUnlinkFxaAccount", "command", function () {
      /* no warning as account can't have previously synced */
      gSyncPane.unlinkFirefoxAccount(false);
    });
    setEventListener("rejectReSignIn", "command",
      gSyncPane.reSignIn);
    setEventListener("rejectUnlinkFxaAccount", "command", function () {
      gSyncPane.unlinkFirefoxAccount(true);
    });
    setEventListener("tosPP-small-ToS", "click", gSyncPane.openToS);
    setEventListener("tosPP-small-PP", "click", gSyncPane.openPrivacyPolicy);
    setEventListener("sync-migrate-upgrade", "click", function () {
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      fxaMigrator.createFxAccount(win);
    });
    setEventListener("sync-migrate-unlink", "click", function () {
      gSyncPane.startOverMigration();
    });
    setEventListener("sync-migrate-forget", "click", function () {
      fxaMigrator.forgetFxAccount();
    });
    setEventListener("sync-migrate-resend", "click", function () {
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      fxaMigrator.resendVerificationMail(win);
    });
    setEventListener("fxaSyncComputerName", "keypress", function (e) {
      if (e.keyCode == KeyEvent.DOM_VK_RETURN) {
        document.getElementById("fxaSaveChangeDeviceName").click();
      } else if (e.keyCode == KeyEvent.DOM_VK_ESCAPE) {
        document.getElementById("fxaCancelChangeDeviceName").click();
      }
    });
  },

  _initProfileImageUI: function () {
    try {
      if (Services.prefs.getBoolPref("identity.fxaccounts.profile_image.enabled")) {
        document.getElementById("fxaProfileImage").hidden = false;
      }
    } catch (e) { }
  },

  updateWeavePrefs: function () {
    // ask the migration module to broadcast its current state (and nothing will
    // happen if it's not loaded - which is good, as that means no migration
    // is pending/necessary) - we don't want to suck that module in just to
    // find there's nothing to do.
    Services.obs.notifyObservers(null, "fxa-migration:state-request", null);

    let service = Components.classes["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;
    // service.fxAccountsEnabled is false iff sync is already configured for
    // the legacy provider.
    if (service.fxAccountsEnabled) {
      let displayNameLabel = document.getElementById("fxaDisplayName");
      let fxaEmailAddress1Label = document.getElementById("fxaEmailAddress1");
      fxaEmailAddress1Label.hidden = false;
      displayNameLabel.hidden = true;

      let profileInfoEnabled;
      try {
        profileInfoEnabled = Services.prefs.getBoolPref("identity.fxaccounts.profile_image.enabled");
      } catch (ex) {}

      // determine the fxa status...
      this._showLoadPage(service);

      fxAccounts.getSignedInUser().then(data => {
        if (!data) {
          this.page = FXA_PAGE_LOGGED_OUT;
          return false;
        }
        this.page = FXA_PAGE_LOGGED_IN;
        // We are logged in locally, but maybe we are in a state where the
        // server rejected our credentials (eg, password changed on the server)
        let fxaLoginStatus = document.getElementById("fxaLoginStatus");
        let syncReady;
        // Not Verfied implies login error state, so check that first.
        if (!data.verified) {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_UNVERIFIED;
          syncReady = false;
        // So we think we are logged in, so login problems are next.
        // (Although if the Sync identity manager is still initializing, we
        // ignore login errors and assume all will eventually be good.)
        // LOGIN_FAILED_LOGIN_REJECTED explicitly means "you must log back in".
        // All other login failures are assumed to be transient and should go
        // away by themselves, so aren't reflected here.
        } else if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_FAILED;
          syncReady = false;
        // Else we must be golden (or in an error state we expect to magically
        // resolve itself)
        } else {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_VERIFIED;
          syncReady = true;
        }
        fxaEmailAddress1Label.textContent = data.email;
        document.getElementById("fxaEmailAddress2").textContent = data.email;
        document.getElementById("fxaEmailAddress3").textContent = data.email;
        document.getElementById("fxaSyncComputerName").value = Weave.Service.clientsEngine.localName;
        let engines = document.getElementById("fxaSyncEngines")
        for (let checkbox of engines.querySelectorAll("checkbox")) {
          checkbox.disabled = !syncReady;
        }
        document.getElementById("fxaChangeDeviceName").disabled = !syncReady;

        // Clear the profile image (if any) of the previously logged in account.
        document.getElementById("fxaProfileImage").style.removeProperty("list-style-image");

        // If the account is verified the next promise in the chain will
        // fetch profile data.
        return data.verified;
      }).then(isVerified => {
        if (isVerified) {
          return fxAccounts.getSignedInUserProfile();
        }
      }).then(data => {
        if (data && profileInfoEnabled) {
          if (data.displayName) {
            fxaEmailAddress1Label.hidden = true;
            displayNameLabel.hidden = false;
            displayNameLabel.textContent = data.displayName;
          }
          if (data.avatar) {
            // Make sure the image is available before displaying it,
            // as we don't want to overwrite the default profile image
            // with a broken/unavailable image
            let img = new Image();
            img.onload = () => {
              let bgImage = "url('" + data.avatar + "')";
              document.getElementById("fxaProfileImage").style.listStyleImage = bgImage;
            };
            img.src = data.avatar;
          }
        }
      }, err => {
        FxAccountsCommon.log.error(err);
      }).catch(err => {
        // If we get here something's really busted
        Cu.reportError(String(err));
      });

    // If fxAccountEnabled is false and we are in a "not configured" state,
    // then fxAccounts is probably fully disabled rather than just unconfigured,
    // so handle this case.  This block can be removed once we remove support
    // for fxAccounts being disabled.
    } else if (Weave.Status.service == Weave.CLIENT_NOT_CONFIGURED ||
               Weave.Svc.Prefs.get("firstSync", "") == "notReady") {
      this.page = PAGE_NO_ACCOUNT;
    // else: sync was previously configured for the legacy provider, so we
    // make the "old" panels available.
    } else if (Weave.Status.login == Weave.LOGIN_FAILED_INVALID_PASSPHRASE ||
               Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
      this.needsUpdate();
    } else {
      this.page = PAGE_HAS_ACCOUNT;
      document.getElementById("accountName").textContent = Weave.Service.identity.account;
      document.getElementById("syncComputerName").value = Weave.Service.clientsEngine.localName;
      document.getElementById("tosPP-normal").hidden = this._usingCustomServer;
    }
  },

  updateMigrationState: function(subject, state) {
    this._closeSyncStatusMessageBox();
    let selIndex;
    let sb = this._accountsStringBundle;
    switch (state) {
      case fxaMigrator.STATE_USER_FXA: {
        // There are 2 cases here - no email address means it is an offer on
        // the first device (so the user is prompted to create an account).
        // If there is an email address it is the "join the party" flow, so the
        // user is prompted to sign in with the address they previously used.
        let email = subject ? subject.QueryInterface(Components.interfaces.nsISupportsString).data : null;
        let elt = document.getElementById("syncStatusMessageDescription");
        elt.textContent = email ?
                          sb.formatStringFromName("signInAfterUpgradeOnOtherDevice.description",
                                                  [email], 1) :
                          sb.GetStringFromName("needUserLong");

        // The "Learn more" link.
        if (!email) {
          let learnMoreLink = document.createElement("label");
          learnMoreLink.id = "learnMoreLink";
          learnMoreLink.className = "text-link";
          let { text, href } = fxaMigrator.learnMoreLink;
          learnMoreLink.setAttribute("value", text);
          learnMoreLink.href = href;
          elt.parentNode.insertBefore(learnMoreLink, elt.nextSibling);
        }

        // The "upgrade" button.
        let button = document.getElementById("sync-migrate-upgrade");
        button.setAttribute("label",
                            sb.GetStringFromName(email
                                                 ? "signInAfterUpgradeOnOtherDevice.label"
                                                 : "upgradeToFxA.label"));
        button.setAttribute("accesskey",
                            sb.GetStringFromName(email
                                                 ? "signInAfterUpgradeOnOtherDevice.accessKey"
                                                 : "upgradeToFxA.accessKey"));
        // The "unlink" button - this is only shown for first migration
        button = document.getElementById("sync-migrate-unlink");
        if (email) {
          button.hidden = true;
        } else {
          button.setAttribute("label", sb.GetStringFromName("unlinkMigration.label"));
          button.setAttribute("accesskey", sb.GetStringFromName("unlinkMigration.accessKey"));
        }
        selIndex = 0;
        break;
      }
      case fxaMigrator.STATE_USER_FXA_VERIFIED: {
        let sb = this._accountsStringBundle;
        let email = subject.QueryInterface(Components.interfaces.nsISupportsString).data;
        let label = sb.formatStringFromName("needVerifiedUserLong", [email], 1);
        let elt = document.getElementById("syncStatusMessageDescription");
        elt.setAttribute("value", label);
        // The "resend" button.
        let button = document.getElementById("sync-migrate-resend");
        button.setAttribute("label", sb.GetStringFromName("resendVerificationEmail.label"));
        button.setAttribute("accesskey", sb.GetStringFromName("resendVerificationEmail.accessKey"));
        // The "forget" button.
        button = document.getElementById("sync-migrate-forget");
        button.setAttribute("label", sb.GetStringFromName("forgetMigration.label"));
        button.setAttribute("accesskey", sb.GetStringFromName("forgetMigration.accessKey"));
        selIndex = 1;
        break;
      }
      default:
        if (state) { // |null| is expected, but everything else is not.
          Cu.reportError("updateMigrationState has unknown state: " + state);
        }
        document.getElementById("sync-migration").hidden = true;
        return;
    }
    document.getElementById("sync-migration-buttons-deck").selectedIndex = selIndex;
    document.getElementById("syncStatusMessage").setAttribute("message-type", "migration");
  },

  // Called whenever one of the sync engine preferences is changed.
  onPreferenceChanged: function() {
    let prefElts = document.querySelectorAll("#syncEnginePrefs > preference");
    let syncEnabled = false;
    for (let elt of prefElts) {
      if (elt.name.startsWith("services.sync.") && elt.value) {
        syncEnabled = true;
        break;
      }
    }
    Services.prefs.setBoolPref("services.sync.enabled", syncEnabled);
  },

  startOver: function (showDialog) {
    if (showDialog) {
      let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                  Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL + 
                  Services.prompt.BUTTON_POS_1_DEFAULT;
      let buttonChoice =
        Services.prompt.confirmEx(window,
                                  this._stringBundle.GetStringFromName("syncUnlink.title"),
                                  this._stringBundle.GetStringFromName("syncUnlink.label"),
                                  flags,
                                  this._stringBundle.GetStringFromName("syncUnlinkConfirm.label"),
                                  null, null, null, {});

      // If the user selects cancel, just bail
      if (buttonChoice == 1)
        return;
    }

    Weave.Service.startOver();
    this.updateWeavePrefs();
  },

  // When the "Unlink" button in the migration header is selected we display
  // a slightly different message.
  startOverMigration: function () {
    let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL +
                Services.prompt.BUTTON_POS_1_DEFAULT;
    let sb = this._accountsStringBundle;
    let buttonChoice =
      Services.prompt.confirmEx(window,
                                sb.GetStringFromName("unlinkVerificationTitle"),
                                sb.GetStringFromName("unlinkVerificationDescription"),
                                flags,
                                sb.GetStringFromName("unlinkVerificationConfirm"),
                                null, null, null, {});

    // If the user selects cancel, just bail
    if (buttonChoice == 1)
      return;

    fxaMigrator.recordTelemetry(fxaMigrator.TELEMETRY_UNLINKED);
    Weave.Service.startOver();
    this.updateWeavePrefs();
  },

  updatePass: function () {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED)
      gSyncUtils.changePassword();
    else
      gSyncUtils.updatePassphrase();
  },

  resetPass: function () {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED)
      gSyncUtils.resetPassword();
    else
      gSyncUtils.resetPassphrase();
  },

  _getEntryPoint: function () {
    let params = new URLSearchParams(document.URL.split("#")[0].split("?")[1] || "");
    return params.get("entrypoint") || "preferences";
  },

  _openAboutAccounts: function(action) {
    let entryPoint = this._getEntryPoint();
    let params = new URLSearchParams();
    if (action) {
      params.set("action", action);
    }
    params.set("entrypoint", entryPoint);

    this.replaceTabWithUrl("about:accounts?" + params);
  },

  /**
   * Invoke the Sync setup wizard.
   *
   * @param wizardType
   *        Indicates type of wizard to launch:
   *          null    -- regular set up wizard
   *          "pair"  -- pair a device first
   *          "reset" -- reset sync
   */
  openSetup: function (wizardType) {
    let service = Components.classes["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;

    if (service.fxAccountsEnabled) {
      this._openAboutAccounts();
    } else {
      let win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
      if (win)
        win.focus();
      else {
        window.openDialog("chrome://browser/content/sync/setup.xul",
                          "weaveSetup", "centerscreen,chrome,resizable=no",
                          wizardType);
      }
    }
  },

  openContentInBrowser: function(url, options) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win) {
      // no window to use, so use _openLink to create a new one.  We don't
      // always use that as it prefers to open a new window rather than use
      // an existing one.
      gSyncUtils._openLink(url);
      return;
    }
    win.switchToTabHavingURI(url, true, options);
  },

  // Replace the current tab with the specified URL.
  replaceTabWithUrl(url) {
    // Get the <browser> element hosting us.
    let browser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell)
                        .chromeEventHandler;
    // And tell it to load our URL.
    browser.loadURI(url);
  },

  openPrivacyPolicy: function(aEvent) {
    aEvent.stopPropagation();
    gSyncUtils.openPrivacyPolicy();
  },

  openToS: function(aEvent) {
    aEvent.stopPropagation();
    gSyncUtils.openToS();
  },

  signUp: function() {
    this._openAboutAccounts("signup");
  },

  signIn: function() {
    this._openAboutAccounts("signin");
  },

  reSignIn: function() {
    this._openAboutAccounts("reauth");
  },

  openChangeProfileImage: function() {
    fxAccounts.promiseAccountsChangeProfileURI(this._getEntryPoint(), "avatar")
      .then(url => {
        this.openContentInBrowser(url, {
          replaceQueryString: true
        });
      });
  },

  manageFirefoxAccount: function() {
    fxAccounts.promiseAccountsManageURI(this._getEntryPoint())
      .then(url => {
        this.openContentInBrowser(url, {
          replaceQueryString: true
        });
      });
  },

  verifyFirefoxAccount: function() {
    this._closeSyncStatusMessageBox();
    let changesyncStatusMessage = (data) => {
      let isError = !data;
      let syncStatusMessage = document.getElementById("syncStatusMessage");
      let syncStatusMessageTitle = document.getElementById("syncStatusMessageTitle");
      let syncStatusMessageDescription = document.getElementById("syncStatusMessageDescription");
      let maybeNot = isError ? "Not" : "";
      let sb = this._accountsStringBundle;
      let title = sb.GetStringFromName("verification" + maybeNot + "SentTitle");
      let email = !isError && data ? data.email : "";
      let description = sb.formatStringFromName("verification" + maybeNot + "SentFull", [email], 1)

      syncStatusMessageTitle.textContent = title;
      syncStatusMessageDescription.textContent = description;
      let messageType = isError ? "verify-error" : "verify-success";
      syncStatusMessage.setAttribute("message-type", messageType);
    }

    let onError = () => {
      changesyncStatusMessage();
    };

    let onSuccess = data => {
      if (data) {
        changesyncStatusMessage(data);
      } else {
        onError();
      }
    };

    fxAccounts.resendVerificationEmail()
      .then(fxAccounts.getSignedInUser, onError)
      .then(onSuccess, onError);
  },

  openOldSyncSupportPage: function() {
    let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "old-sync";
    this.openContentInBrowser(url);
  },

  unlinkFirefoxAccount: function(confirm) {
    if (confirm) {
      // We use a string bundle shared with aboutAccounts.
      let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
      let continueLabel = sb.GetStringFromName("continue.label");
      let title = sb.GetStringFromName("disconnect.verify.title");
      let brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
      let brandShortName = brandBundle.GetStringFromName("brandShortName");
      let body = sb.GetStringFromName("disconnect.verify.heading") +
                 "\n\n" +
                 sb.formatStringFromName("disconnect.verify.description",
                                         [brandShortName], 1);
      let ps = Services.prompt;
      let buttonFlags = (ps.BUTTON_POS_0 * ps.BUTTON_TITLE_IS_STRING) +
                        (ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL) +
                        ps.BUTTON_POS_1_DEFAULT;

      let factory = Cc["@mozilla.org/prompter;1"]
                      .getService(Ci.nsIPromptFactory);
      let prompt = factory.getPrompt(window, Ci.nsIPrompt);
      let bag = prompt.QueryInterface(Ci.nsIWritablePropertyBag2);
      bag.setPropertyAsBool("allowTabModal", true);

      let pressed = prompt.confirmEx(title, body, buttonFlags,
                                     continueLabel, null, null, null, {});

      if (pressed != 0) { // 0 is the "continue" button
        return;
      }
    }
    fxAccounts.signOut().then(() => {
      this.updateWeavePrefs();
    });
  },

  openAddDevice: function () {
    if (!Weave.Utils.ensureMPUnlocked())
      return;
    
    let win = Services.wm.getMostRecentWindow("Sync:AddDevice");
    if (win)
      win.focus();
    else 
      window.openDialog("chrome://browser/content/sync/addDevice.xul",
                        "syncAddDevice", "centerscreen,chrome,resizable=no");
  },

  resetSync: function () {
    this.openSetup("reset");
  },
};

