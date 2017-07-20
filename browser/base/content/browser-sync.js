/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://services-sync/UIState.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Weave",
  "resource://services-sync/main.js");

const MIN_STATUS_ANIMATION_DURATION = 1600;

var gSync = {
  _initialized: false,
  // The last sync start time. Used to calculate the leftover animation time
  // once syncing completes (bug 1239042).
  _syncStartTime: 0,
  _syncAnimationTimer: 0,

  _obs: [
    "weave:engine:sync:finish",
    "quit-application",
    UIState.ON_UPDATE
  ],

  get fxaStrings() {
    delete this.fxaStrings;
    return this.fxaStrings = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
  },

  get syncStrings() {
    delete this.syncStrings;
    // XXXzpao these strings should probably be moved from /services to /browser... (bug 583381)
    //        but for now just make it work
    return this.syncStrings = Services.strings.createBundle(
      "chrome://weave/locale/sync.properties"
    );
  },

  get syncReady() {
    return Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject.ready;
  },

  // Returns true if sync is configured but hasn't loaded or is yet to determine
  // if any remote clients exist.
  get syncConfiguredAndLoading() {
    return UIState.get().status == UIState.STATUS_SIGNED_IN &&
           (!this.syncReady ||
           // lastSync will be non-zero after the first sync
           Weave.Service.clientsEngine.lastSync == 0);
  },

  get isSignedIn() {
    return UIState.get().status == UIState.STATUS_SIGNED_IN;
  },

  get remoteClients() {
    return Weave.Service.clientsEngine.remoteClients
           .sort((a, b) => a.name.localeCompare(b.name));
  },

  _generateNodeGetters(usePhoton) {
    let prefix = usePhoton ? "appMenu-" : "PanelUI-";
    for (let k of ["Status", "Avatar", "Label", "Container"]) {
      let prop = "appMenu" + k;
      let suffix = k.toLowerCase();
      delete this[prop];
      this.__defineGetter__(prop, function() {
        delete this[prop];
        return this[prop] = document.getElementById(prefix + "fxa-" + suffix);
      });
    }
  },

  _maybeUpdateUIState() {
    // Update the UI.
    if (UIState.isReady()) {
      const state = UIState.get();
      // If we are not configured, the UI is already in the right state when
      // we open the window. We can avoid a repaint.
      if (state.status != UIState.STATUS_NOT_CONFIGURED) {
        this.updateAllUI(state);
      }
    }

    this.maybeMoveSyncedTabsButton();
  },

  init() {
    // Bail out if we're already initialized or for pop-up windows.
    if (this._initialized || !window.toolbar.visible) {
      return;
    }

    for (let topic of this._obs) {
      Services.obs.addObserver(this, topic, true);
    }

    // Use this getter not because 'lazy' but because of the observer,
    // which lets us easily update our UI according to the pref flip
    XPCOMUtils.defineLazyPreferenceGetter(this, "gPhotonStructure",
      "browser.photon.structure.enabled", (pref, old, newValue) => {
      this._generateNodeGetters(newValue);
      this._maybeUpdateUIState();
    });
    this._generateNodeGetters(this.gPhotonStructure);

    // initial label for the sync buttons.
    let broadcaster = document.getElementById("sync-status");
    broadcaster.setAttribute("label", this.syncStrings.GetStringFromName("syncnow.label"));

    this._maybeUpdateUIState();

    EnsureFxAccountsWebChannel();

    this._initialized = true;
  },

  uninit() {
    if (!this._initialized) {
      return;
    }

    for (let topic of this._obs) {
      Services.obs.removeObserver(this, topic);
    }

    this._initialized = false;
  },

  observe(subject, topic, data) {
    if (!this._initialized) {
      Cu.reportError("browser-sync observer called after unload: " + topic);
      return;
    }
    switch (topic) {
      case UIState.ON_UPDATE:
        const state = UIState.get();
        this.updateAllUI(state);
        break;
      case "quit-application":
        // Stop the animation timer on shutdown, since we can't update the UI
        // after this.
        clearTimeout(this._syncAnimationTimer);
        break;
      case "weave:engine:sync:finish":
        if (data != "clients") {
          return;
        }
        this.onClientsSynced();
        break;
    }
  },

  updateAllUI(state) {
    this.updatePanelPopup(state);
    this.updateStateBroadcasters(state);
    this.updateSyncButtonsTooltip(state);
    this.updateSyncStatus(state);
  },

  updatePanelPopup(state) {
    let defaultLabel = this.appMenuStatus.getAttribute("defaultlabel");
    // The localization string is for the signed in text, but it's the default text as well
    let defaultTooltiptext = this.appMenuStatus.getAttribute("signedinTooltiptext");

    const status = state.status;
    // Reset the status bar to its original state.
    this.appMenuLabel.setAttribute("label", defaultLabel);
    this.appMenuStatus.setAttribute("tooltiptext", defaultTooltiptext);
    this.appMenuContainer.removeAttribute("fxastatus");
    this.appMenuAvatar.style.removeProperty("list-style-image");

    if (status == UIState.STATUS_NOT_CONFIGURED) {
      return;
    }

    // At this point we consider sync to be configured (but still can be in an error state).
    if (status == UIState.STATUS_LOGIN_FAILED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName("reconnectDescription", [state.email], 1);
      let errorLabel = this.appMenuStatus.getAttribute("errorlabel");
      this.appMenuContainer.setAttribute("fxastatus", "login-failed");
      this.appMenuLabel.setAttribute("label", errorLabel);
      this.appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      return;
    } else if (status == UIState.STATUS_NOT_VERIFIED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName("verifyDescription", [state.email], 1);
      let unverifiedLabel = this.appMenuStatus.getAttribute("unverifiedlabel");
      this.appMenuContainer.setAttribute("fxastatus", "unverified");
      this.appMenuLabel.setAttribute("label", unverifiedLabel);
      this.appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      return;
    }

    // At this point we consider sync to be logged-in.
    this.appMenuContainer.setAttribute("fxastatus", "signedin");
    this.appMenuLabel.setAttribute("label", state.displayName || state.email);

    if (state.avatarURL) {
      let bgImage = "url(\"" + state.avatarURL + "\")";
      this.appMenuAvatar.style.listStyleImage = bgImage;

      let img = new Image();
      img.onerror = () => {
        // Clear the image if it has trouble loading. Since this callback is asynchronous
        // we check to make sure the image is still the same before we clear it.
        if (this.appMenuAvatar.style.listStyleImage === bgImage) {
          this.appMenuAvatar.style.removeProperty("list-style-image");
        }
      };
      img.src = state.avatarURL;
    }
  },

  updateStateBroadcasters(state) {
    const status = state.status;

    // Start off with a clean slate
    document.getElementById("sync-reauth-state").hidden = true;
    document.getElementById("sync-setup-state").hidden = true;
    document.getElementById("sync-syncnow-state").hidden = true;

    if (status == UIState.STATUS_LOGIN_FAILED) {
      // unhiding this element makes the menubar show the login failure state.
      document.getElementById("sync-reauth-state").hidden = false;
    } else if (status == UIState.STATUS_NOT_CONFIGURED ||
               status == UIState.STATUS_NOT_VERIFIED) {
      document.getElementById("sync-setup-state").hidden = false;
    } else {
      document.getElementById("sync-syncnow-state").hidden = false;
    }
  },

  updateSyncStatus(state) {
    const broadcaster = document.getElementById("sync-status");
    const syncingUI = broadcaster.getAttribute("syncstatus") == "active";
    if (state.syncing != syncingUI) { // Do we need to update the UI?
      state.syncing ? this.onActivityStart() : this.onActivityStop();
    }
  },

  onMenuPanelCommand() {
    switch (this.appMenuContainer.getAttribute("fxastatus")) {
    case "signedin":
      this.openPrefs("menupanel", "fxaSignedin");
      break;
    case "error":
      if (this.appMenuContainer.getAttribute("fxastatus") == "unverified") {
        this.openPrefs("menupanel", "fxaError");
      } else {
        this.openSignInAgainPage("menupanel");
      }
      break;
    default:
      this.openPrefs("menupanel", "fxa");
      break;
    }

    PanelUI.hide();
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
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  openSignInAgainPage(entryPoint) {
    this.openAccountsPage("reauth", { entrypoint: entryPoint });
  },

  async openDevicesManagementPage(entryPoint) {
    let url = await fxAccounts.promiseAccountsManageDevicesURI(entryPoint);
    switchToTabHavingURI(url, true, {
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  openSendToDevicePromo() {
    let url = Services.prefs.getCharPref("app.productInfo.baseURL");
    url += "send-tabs/?utm_source=" + Services.appinfo.name.toLowerCase();
    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  sendTabToDevice(url, clientId, title) {
    Weave.Service.clientsEngine.sendURIToClientForDisplay(url, clientId, title).catch(e => {
      console.error("Could not send tab to device", e);
    });
  },

  populateSendTabToDevicesMenu(devicesPopup, url, title, createDeviceNodeFn) {
    if (!createDeviceNodeFn) {
      createDeviceNodeFn = (clientId, name, clientType) => {
        let eltName = name ? "menuitem" : "menuseparator";
        return document.createElement(eltName);
      };
    }

    // remove existing menu items
    for (let i = devicesPopup.childNodes.length - 1; i >= 0; --i) {
      let child = devicesPopup.childNodes[i];
      if (child.classList.contains("sync-menuitem")) {
        child.remove();
      }
    }

    if (gSync.syncConfiguredAndLoading) {
      // We can only be in this case in the page action menu.
      return;
    }

    const fragment = document.createDocumentFragment();

    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN && this.remoteClients.length > 0) {
      this._appendSendTabDeviceList(fragment, createDeviceNodeFn, url, title);
    } else if (state.status == UIState.STATUS_SIGNED_IN) {
      this._appendSendTabSingleDevice(fragment, createDeviceNodeFn);
    } else if (state.status == UIState.STATUS_NOT_VERIFIED ||
               state.status == UIState.STATUS_LOGIN_FAILED) {
      this._appendSendTabVerify(fragment, createDeviceNodeFn);
    } else /* status is STATUS_NOT_CONFIGURED */ {
      this._appendSendTabUnconfigured(fragment, createDeviceNodeFn);
    }

    devicesPopup.appendChild(fragment);
  },

  _appendSendTabDeviceList(fragment, createDeviceNodeFn, url, title) {
    const onTargetDeviceCommand = (event) => {
      let clients = event.target.getAttribute("clientId") ?
        [event.target.getAttribute("clientId")] :
        this.remoteClients.map(client => client.id);

      clients.forEach(clientId => this.sendTabToDevice(url, clientId, title));
      gPageActionButton.panel.hidePopup();
    }

    function addTargetDevice(clientId, name, clientType) {
      const targetDevice = createDeviceNodeFn(clientId, name, clientType);
      targetDevice.addEventListener("command", onTargetDeviceCommand, true);
      targetDevice.classList.add("sync-menuitem", "sendtab-target");
      targetDevice.setAttribute("clientId", clientId);
      targetDevice.setAttribute("clientType", clientType);
      targetDevice.setAttribute("label", name);
      fragment.appendChild(targetDevice);
    }

    const clients = this.remoteClients;
    for (let client of clients) {
      addTargetDevice(client.id, client.name, client.type);
    }

    // "Send to All Devices" menu item
    if (clients.length > 1) {
      const separator = createDeviceNodeFn();
      separator.classList.add("sync-menuitem");
      fragment.appendChild(separator);
      const allDevicesLabel = this.fxaStrings.GetStringFromName("sendToAllDevices.menuitem");
      addTargetDevice("", allDevicesLabel, "");
    }
  },

  _appendSendTabSingleDevice(fragment, createDeviceNodeFn) {
    const noDevices = this.fxaStrings.GetStringFromName("sendTabToDevice.singledevice.status");
    const learnMore = this.fxaStrings.GetStringFromName("sendTabToDevice.singledevice");
    this._appendSendTabInfoItems(fragment, createDeviceNodeFn, noDevices, learnMore, () => {
      this.openSendToDevicePromo();
      gPageActionButton.panel.hidePopup();
    });
  },

  _appendSendTabVerify(fragment, createDeviceNodeFn) {
    const notVerified = this.fxaStrings.GetStringFromName("sendTabToDevice.verify.status");
    const verifyAccount = this.fxaStrings.GetStringFromName("sendTabToDevice.verify");
    this._appendSendTabInfoItems(fragment, createDeviceNodeFn, notVerified, verifyAccount, () => {
      this.openPrefs("sendtab");
      gPageActionButton.panel.hidePopup();
    });
  },

  _appendSendTabUnconfigured(fragment, createDeviceNodeFn) {
    const notConnected = this.fxaStrings.GetStringFromName("sendTabToDevice.unconfigured.status");
    const learnMore = this.fxaStrings.GetStringFromName("sendTabToDevice.unconfigured");
    this._appendSendTabInfoItems(fragment, createDeviceNodeFn, notConnected, learnMore, () => {
      this.openSendToDevicePromo();
      gPageActionButton.panel.hidePopup();
    });
  },

  _appendSendTabInfoItems(fragment, createDeviceNodeFn, statusLabel, actionLabel, actionCommand) {
    const status = createDeviceNodeFn(null, statusLabel, null);
    status.setAttribute("label", statusLabel);
    status.setAttribute("disabled", true);
    status.classList.add("sync-menuitem");
    fragment.appendChild(status);

    const separator = createDeviceNodeFn(null, null, null);
    separator.classList.add("sync-menuitem");
    fragment.appendChild(separator);

    const actionItem = createDeviceNodeFn(null, actionLabel, null);
    actionItem.addEventListener("command", actionCommand, true);
    actionItem.classList.add("sync-menuitem");
    actionItem.setAttribute("label", actionLabel);
    fragment.appendChild(actionItem);
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

  // "Send Tab to Device" menu item
  updateTabContextMenu(aPopupMenu, aTargetTab) {
    const enabled = !this.syncConfiguredAndLoading &&
                    this.isSendableURI(aTargetTab.linkedBrowser.currentURI.spec);

    document.getElementById("context_sendTabToDevice").disabled = !enabled;
  },

  // "Send Page to Device" and "Send Link to Device" menu items
  updateContentContextMenu(contextMenu) {
    // showSendLink and showSendPage are mutually exclusive
    const showSendLink = contextMenu.onSaveableLink || contextMenu.onPlainTextLink;
    const showSendPage = !showSendLink
                         && !(contextMenu.isContentSelected ||
                              contextMenu.onImage || contextMenu.onCanvas ||
                              contextMenu.onVideo || contextMenu.onAudio ||
                              contextMenu.onLink || contextMenu.onTextInput);

    ["context-sendpagetodevice", "context-sep-sendpagetodevice"]
    .forEach(id => contextMenu.showItem(id, showSendPage));
    ["context-sendlinktodevice", "context-sep-sendlinktodevice"]
    .forEach(id => contextMenu.showItem(id, showSendLink));

    if (!showSendLink && !showSendPage) {
      return;
    }

    const targetURI = showSendLink ? contextMenu.linkURL :
                                     contextMenu.browser.currentURI.spec;
    const enabled = !this.syncConfiguredAndLoading && this.isSendableURI(targetURI);
    contextMenu.setItemAttr(showSendPage ? "context-sendpagetodevice" :
                                           "context-sendlinktodevice",
                                           "disabled", !enabled || null);
  },

  // Functions called by observers
  onActivityStart() {
    clearTimeout(this._syncAnimationTimer);
    this._syncStartTime = Date.now();

    let broadcaster = document.getElementById("sync-status");
    broadcaster.setAttribute("syncstatus", "active");
    broadcaster.setAttribute("label", this.syncStrings.GetStringFromName("syncing2.label"));
    broadcaster.setAttribute("disabled", "true");
  },

  _onActivityStop() {
    if (!gBrowser)
      return;
    let broadcaster = document.getElementById("sync-status");
    broadcaster.removeAttribute("syncstatus");
    broadcaster.removeAttribute("disabled");
    broadcaster.setAttribute("label", this.syncStrings.GetStringFromName("syncnow.label"));
    Services.obs.notifyObservers(null, "test:browser-sync:activity-stop");
  },

  onActivityStop() {
    let now = Date.now();
    let syncDuration = now - this._syncStartTime;

    if (syncDuration < MIN_STATUS_ANIMATION_DURATION) {
      let animationTime = MIN_STATUS_ANIMATION_DURATION - syncDuration;
      clearTimeout(this._syncAnimationTimer);
      this._syncAnimationTimer = setTimeout(() => this._onActivityStop(), animationTime);
    } else {
      this._onActivityStop();
    }
  },

  // doSync forces a sync - it *does not* return a promise as it is called
  // via the various UI components.
  doSync() {
    if (!UIState.isReady()) {
      return;
    }
    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN) {
      setTimeout(() => Weave.Service.errorHandler.syncAndReportErrors(), 0);
    }
  },

  openPrefs(entryPoint = "syncbutton", origin = undefined) {
    window.openPreferences("paneSync", { origin, urlParams: { entrypoint: entryPoint } });
  },

  openSyncedTabsPanel() {
    let placement = CustomizableUI.getPlacementOfWidget("sync-button");
    let area = placement ? placement.area : CustomizableUI.AREA_NAVBAR;
    let anchor = document.getElementById("sync-button") ||
                 document.getElementById("PanelUI-menu-button");
    if (area == CustomizableUI.AREA_PANEL) {
      // The button is in the panel, so we need to show the panel UI, then our
      // subview.
      PanelUI.show().then(() => {
        PanelUI.showSubView("PanelUI-remotetabs", anchor, area);
      }).catch(Cu.reportError);
    } else {
      // It is placed somewhere else - just try and show it.
      PanelUI.showSubView("PanelUI-remotetabs", anchor, area);
    }
  },

  /* After we are initialized we perform a once-only check for the sync
     button being in "customize purgatory" and if so, move it to the panel.
     This is done primarily for profiles created before SyncedTabs landed,
     where the button defaulted to being in that purgatory.
     We use a preference to ensure we only do it once, so people can still
     customize it away and have it stick.
  */
  maybeMoveSyncedTabsButton() {
    if (gPhotonStructure) {
      return;
    }
    const prefName = "browser.migrated-sync-button";
    let migrated = Services.prefs.getBoolPref(prefName, false);
    if (migrated) {
      return;
    }
    if (!CustomizableUI.getPlacementOfWidget("sync-button")) {
      CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);
    }
    Services.prefs.setBoolPref(prefName, true);
  },

  /* Update the tooltip for the sync-status broadcaster (which will update the
     Sync Toolbar button and the Sync spinner in the FxA hamburger area.)
     If Sync is configured, the tooltip is when the last sync occurred,
     otherwise the tooltip reflects the fact that Sync needs to be
     (re-)configured.
  */
  updateSyncButtonsTooltip(state) {
    const status = state.status;

    // This is a little messy as the Sync buttons are 1/2 Sync related and
    // 1/2 FxA related - so for some strings we use Sync strings, but for
    // others we reach into gSync for strings.
    let tooltiptext;
    if (status == UIState.STATUS_NOT_VERIFIED) {
      // "needs verification"
      tooltiptext = this.fxaStrings.formatStringFromName("verifyDescription", [state.email], 1);
    } else if (status == UIState.STATUS_NOT_CONFIGURED) {
      // "needs setup".
      tooltiptext = this.syncStrings.GetStringFromName("signInToSync.description");
    } else if (status == UIState.STATUS_LOGIN_FAILED) {
      // "need to reconnect/re-enter your password"
      tooltiptext = this.fxaStrings.formatStringFromName("reconnectDescription", [state.email], 1);
    } else {
      // Sync appears configured - format the "last synced at" time.
      tooltiptext = this.formatLastSyncDate(state.lastSync);
    }

    let broadcaster = document.getElementById("sync-status");
    if (broadcaster) {
      if (tooltiptext) {
        broadcaster.setAttribute("tooltiptext", tooltiptext);
      } else {
        broadcaster.removeAttribute("tooltiptext");
      }
    }
  },

  get withinLastWeekFormat() {
    delete this.withinLastWeekFormat;
    return this.withinLastWeekFormat = new Intl.DateTimeFormat(undefined,
      {weekday: "long", hour: "numeric", minute: "numeric"});
  },

  get oneWeekOrOlderFormat() {
    delete this.oneWeekOrOlderFormat;
    return this.oneWeekOrOlderFormat = new Intl.DateTimeFormat(undefined,
      {month: "long", day: "numeric"});
  },

  formatLastSyncDate(date) {
    let sixDaysAgo = (() => {
      let tempDate = new Date();
      tempDate.setDate(tempDate.getDate() - 6);
      tempDate.setHours(0, 0, 0, 0);
      return tempDate;
    })();

    // It may be confusing for the user to see "Last Sync: Monday" when the last
    // sync was indeed a Monday, but 3 weeks ago.
    let dateFormat = date < sixDaysAgo ? this.oneWeekOrOlderFormat : this.withinLastWeekFormat;

    let lastSyncDateString = dateFormat.format(date);
    return this.syncStrings.formatStringFromName("lastSync2.label", [lastSyncDateString], 1);
  },

  onClientsSynced() {
    let broadcaster = document.getElementById("sync-syncnow-state");
    if (broadcaster) {
      if (Weave.Service.clientsEngine.stats.numClients > 1) {
        broadcaster.setAttribute("devices-status", "multi");
      } else {
        broadcaster.setAttribute("devices-status", "single");
      }
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};
