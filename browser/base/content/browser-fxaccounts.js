/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gFxAccounts = {

  _initialized: false,
  _cachedProfile: null,

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
      "weave:service:sync:error",
      "weave:ui:login:error",
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
    if (!this.weaveService.ready) {
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

  isSendableURI(aURISpec) {
    if (!aURISpec) {
      return false;
    }
    // Disallow sending tabs with more than 65535 characters.
    if (aURISpec.length > 65535) {
      return false;
    }
    try {
      // Filter out un-sendable URIs -- things like local files, object urls, etc.
      const unsendableRegexp = new RegExp(
        Services.prefs.getCharPref("services.sync.engine.tabs.filteredUrls"), "i");
      return !unsendableRegexp.test(aURISpec);
    } catch (e) {
      // The preference has been removed, or is an invalid regexp, so we log an
      // error and treat it as a valid URI -- and the more problematic case is
      // the length, which we've already addressed.
      Cu.reportError(`Failed to build url filter regexp for send tab: ${e}`);
      return true;
    }
  },

  get remoteClients() {
    return Weave.Service.clientsEngine.remoteClients
           .sort((a, b) => a.name.localeCompare(b.name));
  },

  init() {
    // Bail out if we're already initialized and for pop-up windows.
    if (this._initialized || !window.toolbar.visible) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.addObserver(this, topic);
    }

    EnsureFxAccountsWebChannel();
    this._initialized = true;

    this.updateUI();
  },

  uninit() {
    if (!this._initialized) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.removeObserver(this, topic);
    }

    this._initialized = false;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case this.FxAccountsCommon.ON_PROFILE_CHANGE_NOTIFICATION:
        this._cachedProfile = null;
        // Fallthrough intended
      default:
        this.updateUI();
        break;
    }
  },

  // Note that updateUI() returns a Promise that's only used by tests.
  updateUI() {
    this.panelUIFooter.hidden = false;

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
      }
      if (showErrorBadge) {
        PanelUI.showBadgeOnlyNotification("fxa-needs-authentication");
      } else {
        PanelUI.removeNotification("fxa-needs-authentication");
      }
    }

    let updateWithProfile = (profile) => {
      if (profile.displayName) {
        this.panelUILabel.setAttribute("label", profile.displayName);
      }
      if (profile.avatar) {
        let bgImage = "url(\"" + profile.avatar + "\")";
        this.panelUIAvatar.style.listStyleImage = bgImage;

        let img = new Image();
        img.onerror = () => {
          // Clear the image if it has trouble loading. Since this callback is asynchronous
          // we check to make sure the image is still the same before we clear it.
          if (this.panelUIAvatar.style.listStyleImage === bgImage) {
            this.panelUIAvatar.style.removeProperty("list-style-image");
          }
        };
        img.src = profile.avatar;
      }
    }

    return fxAccounts.getSignedInUser().then(userData => {
      // userData may be null here when the user is not signed-in, but that's expected
      updateWithUserData(userData);
      // unverified users cause us to spew log errors fetching an OAuth token
      // to fetch the profile, so don't even try in that case.
      if (!userData || !userData.verified) {
        return null; // don't even try to grab the profile.
      }
      if (this._cachedProfile) {
        return this._cachedProfile;
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
      this._cachedProfile = profile; // Try to avoid fetching the profile on every UI update
    }).catch(error => {
      // This is most likely in tests, were we quickly log users in and out.
      // The most likely scenario is a user logged out, so reflect that.
      // Bug 995134 calls for better errors so we could retry if we were
      // sure this was the failure reason.
      this.FxAccountsCommon.log.error("Error updating FxA account info", error);
      updateWithUserData(null);
    });
  },

  onMenuPanelCommand() {

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

  openPreferences() {
    openPreferences("paneSync", { urlParams: { entrypoint: "menupanel" } });
  },

  openAccountsPage(action, urlParams = {}) {
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

  openSignInAgainPage(entryPoint) {
    this.openAccountsPage("reauth", { entrypoint: entryPoint });
  },

  async openDevicesManagementPage(entryPoint) {
    let url = await fxAccounts.promiseAccountsManageDevicesURI(entryPoint);
    switchToTabHavingURI(url, true, {
      replaceQueryString: true
    });
  },

  sendTabToDevice(url, clientId, title) {
    Weave.Service.clientsEngine.sendURIToClientForDisplay(url, clientId, title);
  },

  populateSendTabToDevicesMenu(devicesPopup, url, title) {
    // remove existing menu items
    while (devicesPopup.hasChildNodes()) {
      devicesPopup.firstChild.remove();
    }

    const fragment = document.createDocumentFragment();

    const onTargetDeviceCommand = (event) => {
      let clients = event.target.getAttribute("clientId") ?
        [event.target.getAttribute("clientId")] :
        this.remoteClients.map(client => client.id);

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

  updateTabContextMenu(aPopupMenu, aTargetTab) {
    if (!this.sendTabToDeviceEnabled ||
        !this.weaveService.ready) {
      return;
    }

    const targetURI = aTargetTab.linkedBrowser.currentURI.spec;
    const showSendTab = this.remoteClients.length > 0 && this.isSendableURI(targetURI);

    ["context_sendTabToDevice", "context_sendTabToDevice_separator"]
    .forEach(id => { document.getElementById(id).hidden = !showSendTab });
  },

  initPageContextMenu(contextMenu) {
    if (!this.sendTabToDeviceEnabled ||
        !this.weaveService.ready) {
      return;
    }

    const remoteClientPresent = this.remoteClients.length > 0;
    // showSendLink and showSendPage are mutually exclusive
    let showSendLink = remoteClientPresent
                       && (contextMenu.onSaveableLink || contextMenu.onPlainTextLink);
    const showSendPage = !showSendLink && remoteClientPresent
                         && !(contextMenu.isContentSelected ||
                              contextMenu.onImage || contextMenu.onCanvas ||
                              contextMenu.onVideo || contextMenu.onAudio ||
                              contextMenu.onLink || contextMenu.onTextInput)
                         && this.isSendableURI(contextMenu.browser.currentURI.spec);

    if (showSendLink) {
      // This isn't part of the condition above since we don't want to try and
      // send the page if a link is clicked on or selected but is not sendable.
      showSendLink = this.isSendableURI(contextMenu.linkURL);
    }

    ["context-sendpagetodevice", "context-sep-sendpagetodevice"]
    .forEach(id => contextMenu.showItem(id, showSendPage));
    ["context-sendlinktodevice", "context-sep-sendlinktodevice"]
    .forEach(id => contextMenu.showItem(id, showSendLink));
  }
};

XPCOMUtils.defineLazyGetter(gFxAccounts, "FxAccountsCommon", function() {
  return Cu.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(this, "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm");


XPCOMUtils.defineLazyGetter(gFxAccounts, "weaveService", function() {
  return Components.classes["@mozilla.org/weave/service;1"]
                   .getService(Components.interfaces.nsISupports)
                   .wrappedJSObject;
});
