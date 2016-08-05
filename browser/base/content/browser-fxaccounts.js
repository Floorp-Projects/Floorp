/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gFxAccounts = {

  SYNC_MIGRATION_NOTIFICATION_TITLE: "fxa-migration",

  _initialized: false,
  _inCustomizationMode: false,

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
      "weave:service:login:change",
      "weave:service:setup-complete",
      "weave:ui:login:error",
      "fxa-migration:state-changed",
      this.FxAccountsCommon.ONLOGIN_NOTIFICATION,
      this.FxAccountsCommon.ONLOGOUT_NOTIFICATION,
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

  get sendTabToDeviceEnabled() {
    return Services.prefs.getBoolPref("services.sync.sendTabToDevice.enabled");
  },

  get remoteClients() {
    return Weave.Service.clientsEngine.remoteClients
           .sort((a, b) => a.name.localeCompare(b.name));
  },

  init: function () {
    // Bail out if we're already initialized and for pop-up windows.
    if (this._initialized || !window.toolbar.visible) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.addObserver(this, topic, false);
    }

    gNavToolbox.addEventListener("customizationstarting", this);
    gNavToolbox.addEventListener("customizationending", this);

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
      case "fxa-migration:state-changed":
        this.onMigrationStateChanged(data, subject);
        break;
      case this.FxAccountsCommon.ONPROFILE_IMAGE_CHANGE_NOTIFICATION:
        this.updateUI();
        break;
      default:
        this.updateUI();
        break;
    }
  },

  onMigrationStateChanged: function () {
    // Since we nuked most of the migration code, this notification will fire
    // once after legacy Sync has been disconnected (and should never fire
    // again)
    let nb = window.document.getElementById("global-notificationbox");

    let msg = this.strings.GetStringFromName("autoDisconnectDescription")
    let signInLabel = this.strings.GetStringFromName("autoDisconnectSignIn.label");
    let signInAccessKey = this.strings.GetStringFromName("autoDisconnectSignIn.accessKey");
    let learnMoreLink = this.fxaMigrator.learnMoreLink;

    let buttons = [
      {
        label: signInLabel,
        accessKey: signInAccessKey,
        callback: () => {
          this.openPreferences();
        }
      }
    ];

    let fragment = document.createDocumentFragment();
    let msgNode = document.createTextNode(msg);
    fragment.appendChild(msgNode);
    if (learnMoreLink) {
      let link = document.createElement("label");
      link.className = "text-link";
      link.setAttribute("value", learnMoreLink.text);
      link.href = learnMoreLink.href;
      fragment.appendChild(link);
    }

    nb.appendNotification(fragment,
                          this.SYNC_MIGRATION_NOTIFICATION_TITLE,
                          undefined,
                          nb.PRIORITY_WARNING_LOW,
                          buttons);

    // ensure the hamburger menu reflects the newly disconnected state.
    this.updateAppMenuItem();
  },

  handleEvent: function (event) {
    this._inCustomizationMode = event.type == "customizationstarting";
    this.updateAppMenuItem();
  },

  updateUI: function () {
    // It's possible someone signed in to FxA after seeing our notification
    // about "Legacy Sync migration" (which now is actually "Legacy Sync
    // auto-disconnect") so kill that notification if it still exists.
    let nb = window.document.getElementById("global-notificationbox");
    let n = nb.getNotificationWithValue(this.SYNC_MIGRATION_NOTIFICATION_TITLE);
    if (n) {
      nb.removeNotification(n, true);
    }

    this.updateAppMenuItem();
  },

  // Note that updateAppMenuItem() returns a Promise that's only used by tests.
  updateAppMenuItem: function () {
    let profileInfoEnabled = false;
    try {
      profileInfoEnabled = Services.prefs.getBoolPref("identity.fxaccounts.profile_image.enabled");
    } catch (e) { }

    // Bail out if FxA is disabled.
    if (!this.weave.fxAccountsEnabled) {
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
    let unverifiedLabel = this.panelUIStatus.getAttribute("unverifiedlabel");
    // The localization string is for the signed in text, but it's the default text as well
    let defaultTooltiptext = this.panelUIStatus.getAttribute("signedinTooltiptext");

    let updateWithUserData = (userData) => {
      // Window might have been closed while fetching data.
      if (window.closed) {
        return;
      }

      // Reset the button to its original state.
      this.panelUILabel.setAttribute("label", defaultLabel);
      this.panelUIStatus.setAttribute("tooltiptext", defaultTooltiptext);
      this.panelUIFooter.removeAttribute("fxastatus");
      this.panelUIFooter.removeAttribute("fxaprofileimage");
      this.panelUIAvatar.style.removeProperty("list-style-image");
      let showErrorBadge = false;
      if (userData) {
        // At this point we consider the user as logged-in (but still can be in an error state)
        if (this.loginFailed) {
          let tooltipDescription = this.strings.formatStringFromName("reconnectDescription", [userData.email], 1);
          this.panelUIFooter.setAttribute("fxastatus", "error");
          this.panelUILabel.setAttribute("label", errorLabel);
          this.panelUIStatus.setAttribute("tooltiptext", tooltipDescription);
          showErrorBadge = true;
        } else if (!userData.verified) {
          let tooltipDescription = this.strings.formatStringFromName("verifyDescription", [userData.email], 1);
          this.panelUIFooter.setAttribute("fxastatus", "error");
          this.panelUIFooter.setAttribute("unverified", "true");
          this.panelUILabel.setAttribute("label", unverifiedLabel);
          this.panelUIStatus.setAttribute("tooltiptext", tooltipDescription);
          showErrorBadge = true;
        } else {
          this.panelUIFooter.setAttribute("fxastatus", "signedin");
          this.panelUILabel.setAttribute("label", userData.email);
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
      if (profileInfoEnabled) {
        if (profile.displayName) {
          this.panelUILabel.setAttribute("label", profile.displayName);
        }
        if (profile.avatar) {
          this.panelUIFooter.setAttribute("fxaprofileimage", "set");
          let bgImage = "url(\"" + profile.avatar + "\")";
          this.panelUIAvatar.style.listStyleImage = bgImage;

          let img = new Image();
          img.onerror = () => {
            // Clear the image if it has trouble loading. Since this callback is asynchronous
            // we check to make sure the image is still the same before we clear it.
            if (this.panelUIAvatar.style.listStyleImage === bgImage) {
              this.panelUIFooter.removeAttribute("fxaprofileimage");
              this.panelUIAvatar.style.removeProperty("list-style-image");
            }
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

  onMenuPanelCommand: function () {

    switch (this.panelUIFooter.getAttribute("fxastatus")) {
    case "signedin":
      this.openPreferences();
      break;
    case "error":
      if (this.panelUIFooter.getAttribute("unverified")) {
        this.openPreferences();
      } else {
        this.openSignInAgainPage("menupanel");
      }
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

  sendTabToDevice: function (url, clientId, title) {
    Weave.Service.clientsEngine.sendURIToClientForDisplay(url, clientId, title);
  },

  populateSendTabToDevicesMenu: function (devicesPopup, url, title) {
    // remove existing menu items
    while (devicesPopup.hasChildNodes()) {
      devicesPopup.removeChild(devicesPopup.firstChild);
    }

    const fragment = document.createDocumentFragment();

    const onTargetDeviceCommand = (event) => {
      const clientId = event.target.getAttribute("clientId");
      const clients = clientId
                      ? [clientId]
                      : this.remoteClients.map(client => client.id);

      clients.forEach(clientId => this.sendTabToDevice(url, clientId, title));
    }

    function addTargetDevice(clientId, name) {
      const targetDevice = document.createElement("menuitem");
      targetDevice.addEventListener("command", onTargetDeviceCommand, true);
      targetDevice.setAttribute("class", "sendtab-target");
      targetDevice.setAttribute("clientId", clientId);
      targetDevice.setAttribute("label", name);
      fragment.appendChild(targetDevice);
    }

    const clients = this.remoteClients;
    for (let client of clients) {
      addTargetDevice(client.id, client.name);
    }

    // "All devices" menu item
    if (clients.length > 1) {
      const separator = document.createElement("menuseparator");
      fragment.appendChild(separator);
      const allDevicesLabel = this.strings.GetStringFromName("sendTabToAllDevices.menuitem");
      addTargetDevice("", allDevicesLabel);
    }

    devicesPopup.appendChild(fragment);
  },

  updateTabContextMenu: function (aPopupMenu) {
    if (!this.sendTabToDeviceEnabled) {
      return;
    }

    const remoteClientPresent = this.remoteClients.length > 0;
    ["context_sendTabToDevice", "context_sendTabToDevice_separator"]
    .forEach(id => { document.getElementById(id).hidden = !remoteClientPresent });
  },

  initPageContextMenu: function (contextMenu) {
    if (!this.sendTabToDeviceEnabled) {
      return;
    }

    const remoteClientPresent = this.remoteClients.length > 0;
    // showSendLink and showSendPage are mutually exclusive
    const showSendLink = remoteClientPresent
                         && (contextMenu.onSaveableLink || contextMenu.onPlainTextLink);
    const showSendPage = !showSendLink && remoteClientPresent
                         && !(contextMenu.isContentSelected ||
                              contextMenu.onImage || contextMenu.onCanvas ||
                              contextMenu.onVideo || contextMenu.onAudio ||
                              contextMenu.onLink || contextMenu.onTextInput);

    ["context-sendpagetodevice", "context-sep-sendpagetodevice"]
    .forEach(id => contextMenu.showItem(id, showSendPage));
    ["context-sendlinktodevice", "context-sep-sendlinktodevice"]
    .forEach(id => contextMenu.showItem(id, showSendLink));
  }
};

XPCOMUtils.defineLazyGetter(gFxAccounts, "FxAccountsCommon", function () {
  return Cu.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(gFxAccounts, "fxaMigrator",
  "resource://services-sync/FxaMigrator.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm");
