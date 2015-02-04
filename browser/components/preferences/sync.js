/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
const PAGE_PLEASE_WAIT = 3;
const FXA_PAGE_LOGGED_OUT = 4;
const FXA_PAGE_LOGGED_IN = 5;

// Indexes into the "login status" deck.
// We are in a successful verified state - everything should work!
const FXA_LOGIN_VERIFIED = 0;
// We have logged in to an unverified account.
const FXA_LOGIN_UNVERIFIED = 1;
// We are logged in locally, but the server rejected our credentials.
const FXA_LOGIN_FAILED = 2;

let gSyncPane = {
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
    label.value = Weave.Utils.getErrorString(Weave.Status.login);
    label.className = "error";
  },

  init: function () {
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
    this.page = PAGE_PLEASE_WAIT;

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

  _init: function () {
    let topics = ["weave:service:login:error",
                  "weave:service:login:finish",
                  "weave:service:start-over:finish",
                  "weave:service:setup-complete",
                  "weave:service:logout:finish",
                  FxAccountsCommon.ONVERIFIED_NOTIFICATION];
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
      topics.forEach(topic => {
        Weave.Svc.Obs.remove(topic, this.updateWeavePrefs, this);
      });
      Weave.Svc.Obs.remove(migrateTopic, this.updateMigrationState, this);
    }.bind(this), false);

    XPCOMUtils.defineLazyGetter(this, '_stringBundle', () => {
      return Services.strings.createBundle("chrome://browser/locale/preferences/preferences.properties");
    });

    XPCOMUtils.defineLazyGetter(this, '_accountsStringBundle', () => {
      return Services.strings.createBundle("chrome://browser/locale/accounts.properties");
    });

    this.updateWeavePrefs();
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
      // determine the fxa status...
      this.page = PAGE_PLEASE_WAIT;
      fxAccounts.getSignedInUser().then(data => {
        if (!data) {
          this.page = FXA_PAGE_LOGGED_OUT;
          return;
        }
        this.page = FXA_PAGE_LOGGED_IN;
        // We are logged in locally, but maybe we are in a state where the
        // server rejected our credentials (eg, password changed on the server)
        let fxaLoginStatus = document.getElementById("fxaLoginStatus");
        let enginesListDisabled;
        // Not Verfied implies login error state, so check that first.
        if (!data.verified) {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_UNVERIFIED;
          enginesListDisabled = true;
        // So we think we are logged in, so login problems are next.
        // (Although if the Sync identity manager is still initializing, we
        // ignore login errors and assume all will eventually be good.)
        // LOGIN_FAILED_LOGIN_REJECTED explicitly means "you must log back in".
        // All other login failures are assumed to be transient and should go
        // away by themselves, so aren't reflected here.
        } else if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_FAILED;
          enginesListDisabled = true;
        // Else we must be golden (or in an error state we expect to magically
        // resolve itself)
        } else {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_VERIFIED;
          enginesListDisabled = false;
        }
        document.getElementById("fxaEmailAddress1").textContent = data.email;
        document.getElementById("fxaEmailAddress2").textContent = data.email;
        document.getElementById("fxaEmailAddress3").textContent = data.email;
        document.getElementById("fxaSyncComputerName").value = Weave.Service.clientsEngine.localName;
        let engines = document.getElementById("fxaSyncEngines")
        for (let checkbox of engines.querySelectorAll("checkbox")) {
          checkbox.disabled = enginesListDisabled;
        }
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
      document.getElementById("accountName").value = Weave.Service.identity.account;
      document.getElementById("syncComputerName").value = Weave.Service.clientsEngine.localName;
      document.getElementById("tosPP-normal").hidden = this._usingCustomServer;
    }
  },

  updateMigrationState: function(subject, state) {
    let selIndex;
    let container = document.getElementById("sync-migration");
    switch (state) {
      case fxaMigrator.STATE_USER_FXA: {
        let sb = this._accountsStringBundle;
        // There are 2 cases here - no email address means it is an offer on
        // the first device (so the user is prompted to create an account).
        // If there is an email address it is the "join the party" flow, so the
        // user is prompted to sign in with the address they previously used.
        let email = subject ? subject.QueryInterface(Components.interfaces.nsISupportsString).data : null;
        let elt = document.getElementById("sync-migrate-upgrade-description");
        elt.textContent = email ?
                          sb.formatStringFromName("signInAfterUpgradeOnOtherDevice.description",
                                                  [email], 1) :
                          sb.GetStringFromName("needUserLong");

        // The "Learn more" link.
        if (!email) {
          let learnMoreLink = document.createElement("label");
          learnMoreLink.className = "text-link";
          let { text, href } = fxaMigrator.learnMoreLink;
          learnMoreLink.setAttribute("value", text);
          learnMoreLink.href = href;
          elt.appendChild(learnMoreLink);
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
        let elt = document.getElementById("sync-migrate-verify-description");
        elt.textContent = label;
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
        if (!container.hidden) {
          window.innerHeight -= container.clientHeight;
          container.hidden = true;
        }
        return;
    }
    document.getElementById("sync-migration-deck").selectedIndex = selIndex;
    if (container.hidden) {
      container.hidden = false;
      window.innerHeight += container.clientHeight;
    }
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
      if (buttonChoice == 1) {
        return;
      }
    }

    Weave.Service.startOver();
    this.updateWeavePrefs();
  },

  updatePass: function () {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
      gSyncUtils.changePassword();
    } else {
      gSyncUtils.updatePassphrase();
    }
  },

  resetPass: function () {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
      gSyncUtils.resetPassword();
    } else {
      gSyncUtils.resetPassphrase();
    }
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
      this.openContentInBrowser("about:accounts?entrypoint=preferences", {
        replaceQueryString: true
      });
    } else {
      let win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
      if (win) {
        win.focus();
      } else {
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
    // seeing as we are doing this in a tab we close the prefs dialog.
    window.close();
  },

  signUp: function() {
    this.openContentInBrowser("about:accounts?action=signup&entrypoint=preferences", {
      replaceQueryString: true
    });
  },

  signIn: function() {
    this.openContentInBrowser("about:accounts?action=signin&entrypoint=preferences", {
      replaceQueryString: true
    });
  },

  reSignIn: function() {
    this.openContentInBrowser("about:accounts?action=reauth&entrypoint=preferences", {
      replaceQueryString: true
    });
  },

  manageFirefoxAccount: function() {
    let url = Services.prefs.getCharPref("identity.fxaccounts.settings.uri");
    this.openContentInBrowser(url);
  },

  verifyFirefoxAccount: function() {
    fxAccounts.resendVerificationEmail().then(() => {
      fxAccounts.getSignedInUser().then(data => {
        let sb = Services.strings.createBundle("chrome://browser/locale/accounts.properties");
        let title = sb.GetStringFromName("verificationSentTitle");
        let heading = sb.formatStringFromName("verificationSentHeading",
                                              [data.email], 1);
        let description = sb.GetStringFromName("verificationSentDescription");

        Services.prompt.alert(window, title, heading + "\n\n" + description);
      });
    });
  },

  openOldSyncSupportPage: function() {
    let url = Services.urlFormatter.formatURLPref('app.support.baseURL') + "old-sync"
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
      let pressed = Services.prompt.confirmEx(window, title, body, buttonFlags,
                                              continueLabel, null, null, null, {});
      if (pressed != 0) { // 0 is the "continue" button
        return;
      }
    }
    fxAccounts.signOut().then(() => {
      this.updateWeavePrefs();
    });
  },

  openQuotaDialog: function () {
    let win = Services.wm.getMostRecentWindow("Sync:ViewQuota");
    if (win) {
      win.focus();
    } else {
      window.openDialog("chrome://browser/content/sync/quota.xul", "",
                        "centerscreen,chrome,dialog,modal");
    }
  },

  openAddDevice: function () {
    if (!Weave.Utils.ensureMPUnlocked()) {
      return;
    }

    let win = Services.wm.getMostRecentWindow("Sync:AddDevice");
    if (win) {
      win.focus();
    } else {
      window.openDialog("chrome://browser/content/sync/addDevice.xul",
                        "syncAddDevice", "centerscreen,chrome,resizable=no");
    }
  },

  resetSync: function () {
    this.openSetup("reset");
  },

  // click handlers for the FxA migration.
  migrateUpgrade: function() {
    fxaMigrator.getFxAccountCreationOptions().then(({url, options}) => {
      this.openContentInBrowser(url, options);
    });
  },

  migrateForget: function() {
    fxaMigrator.forgetFxAccount();
  },

  migrateResend: function() {
    fxaMigrator.resendVerificationMail(window);
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

    Weave.Service.startOver();
    this.updateWeavePrefs();
  },
};

