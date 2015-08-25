# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let gFxAccounts = {

  PREF_SYNC_START_DOORHANGER: "services.sync.ui.showSyncStartDoorhanger",
  DOORHANGER_ACTIVATE_DELAY_MS: 5000,
  SYNC_MIGRATION_NOTIFICATION_TITLE: "fxa-migration",

  _initialized: false,
  _inCustomizationMode: false,
  // _expectingNotifyClose is a hack that helps us determine if the
  // migration notification was closed due to being "dismissed" vs closed
  // due to one of the migration buttons being clicked.  It's ugly and somewhat
  // fragile, so bug 1119020 exists to help us do this better.
  _expectingNotifyClose: false,

  get weave() {
    delete this.weave;
    return this.weave = Cc["@mozilla.org/weave/service;1"]
                          .getService(Ci.nsISupports)
                          .wrappedJSObject;
  },

  get topics() {
    // Do all this dance to lazy-load FxAccountsCommon.
    delete this.topics;
    return this.topics = [
      "weave:service:ready",
      "weave:service:sync:start",
      "weave:service:login:error",
      "weave:service:setup-complete",
      "weave:ui:login:error",
      "fxa-migration:state-changed",
      this.FxAccountsCommon.ONLOGIN_NOTIFICATION,
      this.FxAccountsCommon.ONVERIFIED_NOTIFICATION,
      this.FxAccountsCommon.ONLOGOUT_NOTIFICATION,
      "weave:notification:removed",
      this.FxAccountsCommon.ON_PROFILE_CHANGE_NOTIFICATION,
    ];
  },

  get panelUIFooter() {
    delete this.panelUIFooter;
    return this.panelUIFooter = document.getElementById("PanelUI-footer-fxa");
  },

  get panelUIStatus() {
    delete this.panelUIStatus;
    return this.panelUIStatus = document.getElementById("PanelUI-fxa-status");
  },

  get panelUIAvatar() {
    delete this.panelUIAvatar;
    return this.panelUIAvatar = document.getElementById("PanelUI-fxa-avatar");
  },

  get panelUILabel() {
    delete this.panelUILabel;
    return this.panelUILabel = document.getElementById("PanelUI-fxa-label");
  },

  get panelUIIcon() {
    delete this.panelUIIcon;
    return this.panelUIIcon = document.getElementById("PanelUI-fxa-icon");
  },

  get strings() {
    delete this.strings;
    return this.strings = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
  },

  get loginFailed() {
    // Referencing Weave.Service will implicitly initialize sync, and we don't
    // want to force that - so first check if it is ready.
    let service = Cc["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;
    if (!service.ready) {
      return false;
    }
    // LOGIN_FAILED_LOGIN_REJECTED explicitly means "you must log back in".
    // All other login failures are assumed to be transient and should go
    // away by themselves, so aren't reflected here.
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED;
  },

  get isActiveWindow() {
    let fm = Services.focus;
    return fm.activeWindow == window;
  },

  init: function () {
    // Bail out if we're already initialized and for pop-up windows.
    if (this._initialized || !window.toolbar.visible) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.addObserver(this, topic, false);
    }

    addEventListener("activate", this);
    gNavToolbox.addEventListener("customizationstarting", this);
    gNavToolbox.addEventListener("customizationending", this);

    // Request the current Legacy-Sync-to-FxA migration status.  We'll be
    // notified of fxa-migration:state-changed in response if necessary.
    Services.obs.notifyObservers(null, "fxa-migration:state-request", null);

    EnsureFxAccountsWebChannel();
    this._initialized = true;

    this.updateUI();
  },

  uninit: function () {
    if (!this._initialized) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.removeObserver(this, topic);
    }

    this._initialized = false;
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case this.FxAccountsCommon.ONVERIFIED_NOTIFICATION:
        Services.prefs.setBoolPref(this.PREF_SYNC_START_DOORHANGER, true);
        break;
      case "weave:service:sync:start":
        this.onSyncStart();
        break;
      case "fxa-migration:state-changed":
        this.onMigrationStateChanged(data, subject);
        break;
      case "weave:notification:removed":
        // this exists just so we can tell the difference between "box was
        // closed due to button press" vs "was closed due to click on [x]"
        let notif = subject.wrappedJSObject.object;
        if (notif.title == this.SYNC_MIGRATION_NOTIFICATION_TITLE &&
            !this._expectingNotifyClose) {
          // it's an [x] on our notification, so record telemetry.
          this.fxaMigrator.recordTelemetry(this.fxaMigrator.TELEMETRY_DECLINED);
        }
        break;
      case this.FxAccountsCommon.ONPROFILE_IMAGE_CHANGE_NOTIFICATION:
        this.updateUI();
        break;
      default:
        this.updateUI();
        break;
    }
  },

  onSyncStart: function () {
    if (!this.isActiveWindow) {
      return;
    }

    let showDoorhanger = false;

    try {
      showDoorhanger = Services.prefs.getBoolPref(this.PREF_SYNC_START_DOORHANGER);
    } catch (e) { /* The pref might not exist. */ }

    if (showDoorhanger) {
      Services.prefs.clearUserPref(this.PREF_SYNC_START_DOORHANGER);
      this.showSyncStartedDoorhanger();
    }
  },

  onMigrationStateChanged: function (newState, email) {
    this._migrationInfo = !newState ? null : {
      state: newState,
      email: email ? email.QueryInterface(Ci.nsISupportsString).data : null,
    };
    this.updateUI();
  },

  handleEvent: function (event) {
    if (event.type == "activate") {
      // Our window might have been in the background while we received the
      // sync:start notification. If still needed, show the doorhanger after
      // a short delay. Without this delay the doorhanger would not show up
      // or with a too small delay show up while we're still animating the
      // window.
      setTimeout(() => this.onSyncStart(), this.DOORHANGER_ACTIVATE_DELAY_MS);
    } else {
      this._inCustomizationMode = event.type == "customizationstarting";
      this.updateAppMenuItem();
    }
  },

  showDoorhanger: function (id) {
    let panel = document.getElementById(id);
    let anchor = document.getElementById("PanelUI-menu-button");

    let iconAnchor =
      document.getAnonymousElementByAttribute(anchor, "class",
                                              "toolbarbutton-icon");

    panel.hidden = false;
    panel.openPopup(iconAnchor || anchor, "bottomcenter topright");
  },

  showSyncStartedDoorhanger: function () {
    this.showDoorhanger("sync-start-panel");
  },

  showSyncFailedDoorhanger: function () {
    this.showDoorhanger("sync-error-panel");
  },

  updateUI: function () {
    this.updateAppMenuItem();
    this.updateMigrationNotification();
  },

  // Note that updateAppMenuItem() returns a Promise that's only used by tests.
  updateAppMenuItem: function () {
    if (this._migrationInfo) {
      this.updateAppMenuItemForMigration();
      return Promise.resolve();
    }

    let profileInfoEnabled = false;
    try {
      profileInfoEnabled = Services.prefs.getBoolPref("identity.fxaccounts.profile_image.enabled");
    } catch (e) { }

    // Bail out if FxA is disabled.
    if (!this.weave.fxAccountsEnabled) {
      // When migration transitions from needs-verification to the null state,
      // fxAccountsEnabled is false because migration has not yet finished.  In
      // that case, hide the button.  We'll get another notification with a null
      // state once migration is complete.
      this.panelUIFooter.hidden = true;
      this.panelUIFooter.removeAttribute("fxastatus");
      return Promise.resolve();
    }

    this.panelUIFooter.hidden = false;

    // Make sure the button is disabled in customization mode.
    if (this._inCustomizationMode) {
      this.panelUIStatus.setAttribute("disabled", "true");
      this.panelUILabel.setAttribute("disabled", "true");
      this.panelUIAvatar.setAttribute("disabled", "true");
      this.panelUIIcon.setAttribute("disabled", "true");
    } else {
      this.panelUIStatus.removeAttribute("disabled");
      this.panelUILabel.removeAttribute("disabled");
      this.panelUIAvatar.removeAttribute("disabled");
      this.panelUIIcon.removeAttribute("disabled");
    }

    let defaultLabel = this.panelUIStatus.getAttribute("defaultlabel");
    let errorLabel = this.panelUIStatus.getAttribute("errorlabel");
    let signedInTooltiptext = this.panelUIStatus.getAttribute("signedinTooltiptext");

    let updateWithUserData = (userData) => {
      // Window might have been closed while fetching data.
      if (window.closed) {
        return;
      }

      // Reset the button to its original state.
      this.panelUILabel.setAttribute("label", defaultLabel);
      this.panelUIStatus.removeAttribute("tooltiptext");
      this.panelUIFooter.removeAttribute("fxastatus");
      this.panelUIFooter.removeAttribute("fxaprofileimage");
      this.panelUIAvatar.style.removeProperty("list-style-image");
      let showErrorBadge = false;

      if (!this._inCustomizationMode && userData) {
        // At this point we consider the user as logged-in (but still can be in an error state)
        if (this.loginFailed) {
          let tooltipDescription = this.strings.formatStringFromName("reconnectDescription", [userData.email], 1);
          this.panelUIFooter.setAttribute("fxastatus", "error");
          this.panelUILabel.setAttribute("label", errorLabel);
          this.panelUIStatus.setAttribute("tooltiptext", tooltipDescription);
          showErrorBadge = true;
        } else {
          this.panelUIFooter.setAttribute("fxastatus", "signedin");
          this.panelUILabel.setAttribute("label", userData.email);
          this.panelUIStatus.setAttribute("tooltiptext", signedInTooltiptext);
        }
        if (profileInfoEnabled) {
          this.panelUIFooter.setAttribute("fxaprofileimage", "enabled");
        }
      }
      if (showErrorBadge) {
        gMenuButtonBadgeManager.addBadge(gMenuButtonBadgeManager.BADGEID_FXA, "fxa-needs-authentication");
      } else {
        gMenuButtonBadgeManager.removeBadge(gMenuButtonBadgeManager.BADGEID_FXA);
      }
    }

    let updateWithProfile = (profile) => {
      if (!this._inCustomizationMode && profileInfoEnabled) {
        if (profile.displayName) {
          this.panelUILabel.setAttribute("label", profile.displayName);
        }
        if (profile.avatar) {
          let img = new Image();
          // Make sure the image is available before attempting to display it
          img.onload = () => {
            this.panelUIFooter.setAttribute("fxaprofileimage", "set");
            this.panelUIAvatar.style.listStyleImage = "url('" + profile.avatar + "')";
          };
          img.src = profile.avatar;
        }
      }
    }

    return fxAccounts.getSignedInUser().then(userData => {
      // userData may be null here when the user is not signed-in, but that's expected
      updateWithUserData(userData);
      // unverified users cause us to spew log errors fetching an OAuth token
      // to fetch the profile, so don't even try in that case.
      if (!userData || !userData.verified || !profileInfoEnabled) {
        return null; // don't even try to grab the profile.
      }
      return fxAccounts.getSignedInUserProfile().catch(err => {
        // Not fetching the profile is sad but the FxA logs will already have noise.
        return null;
      });
    }).then(profile => {
      if (!profile) {
        return;
      }
      updateWithProfile(profile);
    }).catch(error => {
      // This is most likely in tests, were we quickly log users in and out.
      // The most likely scenario is a user logged out, so reflect that.
      // Bug 995134 calls for better errors so we could retry if we were
      // sure this was the failure reason.
      this.FxAccountsCommon.log.error("Error updating FxA account info", error);
      updateWithUserData(null);
    });
  },

  updateAppMenuItemForMigration: Task.async(function* () {
    let status = null;
    let label = null;
    switch (this._migrationInfo.state) {
      case this.fxaMigrator.STATE_USER_FXA:
        status = "migrate-signup";
        label = this.strings.formatStringFromName("needUserShort",
          [this.panelUILabel.getAttribute("fxabrandname")], 1);
        break;
      case this.fxaMigrator.STATE_USER_FXA_VERIFIED:
        status = "migrate-verify";
        label = this.strings.formatStringFromName("needVerifiedUserShort",
                                                  [this._migrationInfo.email],
                                                  1);
        break;
    }
    this.panelUILabel.label = label;
    this.panelUIFooter.setAttribute("fxastatus", status);
  }),

  updateMigrationNotification: Task.async(function* () {
    if (!this._migrationInfo) {
      this._expectingNotifyClose = true;
      Weave.Notifications.removeAll(this.SYNC_MIGRATION_NOTIFICATION_TITLE);
      // because this is called even when there is no such notification, we
      // set _expectingNotifyClose back to false as we may yet create a new
      // notification (but in general, once we've created a migration
      // notification once in a session, we don't create one again)
      this._expectingNotifyClose = false;
      return;
    }
    let note = null;
    switch (this._migrationInfo.state) {
      case this.fxaMigrator.STATE_USER_FXA: {
        // There are 2 cases here - no email address means it is an offer on
        // the first device (so the user is prompted to create an account).
        // If there is an email address it is the "join the party" flow, so the
        // user is prompted to sign in with the address they previously used.
        let msg, upgradeLabel, upgradeAccessKey, learnMoreLink;
        if (this._migrationInfo.email) {
          msg = this.strings.formatStringFromName("signInAfterUpgradeOnOtherDevice.description",
                                                  [this._migrationInfo.email],
                                                  1);
          upgradeLabel = this.strings.GetStringFromName("signInAfterUpgradeOnOtherDevice.label");
          upgradeAccessKey = this.strings.GetStringFromName("signInAfterUpgradeOnOtherDevice.accessKey");
        } else {
          msg = this.strings.GetStringFromName("needUserLong");
          upgradeLabel = this.strings.GetStringFromName("upgradeToFxA.label");
          upgradeAccessKey = this.strings.GetStringFromName("upgradeToFxA.accessKey");
          learnMoreLink = this.fxaMigrator.learnMoreLink;
        }
        note = new Weave.Notification(
          undefined, msg, undefined, Weave.Notifications.PRIORITY_WARNING, [
            new Weave.NotificationButton(upgradeLabel, upgradeAccessKey, () => {
              this._expectingNotifyClose = true;
              this.fxaMigrator.createFxAccount(window);
            }),
          ], learnMoreLink
        );
        break;
      }
      case this.fxaMigrator.STATE_USER_FXA_VERIFIED: {
        let msg =
          this.strings.formatStringFromName("needVerifiedUserLong",
                                            [this._migrationInfo.email], 1);
        let resendLabel =
          this.strings.GetStringFromName("resendVerificationEmail.label");
        let resendAccessKey =
          this.strings.GetStringFromName("resendVerificationEmail.accessKey");
        note = new Weave.Notification(
          undefined, msg, undefined, Weave.Notifications.PRIORITY_INFO, [
            new Weave.NotificationButton(resendLabel, resendAccessKey, () => {
              this._expectingNotifyClose = true;
              this.fxaMigrator.resendVerificationMail();
            }),
          ]
        );
        break;
      }
    }
    note.title = this.SYNC_MIGRATION_NOTIFICATION_TITLE;
    Weave.Notifications.replaceTitle(note);
  }),

  onMenuPanelCommand: function () {

    switch (this.panelUIFooter.getAttribute("fxastatus")) {
    case "signedin":
      this.openPreferences();
      break;
    case "error":
      this.openSignInAgainPage("menupanel");
      break;
    case "migrate-signup":
    case "migrate-verify":
      // The migration flow calls for the menu item to open sync prefs rather
      // than requesting migration start immediately.
      this.openPreferences();
      break;
    default:
      this.openPreferences();
      break;
    }

    PanelUI.hide();
  },

  openPreferences: function () {
    openPreferences("paneSync", { urlParams: { entrypoint: "menupanel" } });
  },

  openAccountsPage: function (action, urlParams={}) {
    // An entrypoint param is used for server-side metrics.  If the current tab
    // is UITour, assume that it initiated the call to this method and override
    // the entrypoint accordingly.
    if (UITour.tourBrowsersByWindow.get(window) &&
        UITour.tourBrowsersByWindow.get(window).has(gBrowser.selectedBrowser)) {
      urlParams.entrypoint = "uitour";
    }
    let params = new URLSearchParams();
    if (action) {
      params.set("action", action);
    }
    for (let name in urlParams) {
      if (urlParams[name] !== undefined) {
        params.set(name, urlParams[name]);
      }
    }
    let url = "about:accounts?" + params;
    switchToTabHavingURI(url, true, {
      replaceQueryString: true
    });
  },

  openSignInAgainPage: function (entryPoint) {
    this.openAccountsPage("reauth", { entrypoint: entryPoint });
  },
};

XPCOMUtils.defineLazyGetter(gFxAccounts, "FxAccountsCommon", function () {
  return Cu.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(gFxAccounts, "fxaMigrator",
  "resource://services-sync/FxaMigrator.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm");
