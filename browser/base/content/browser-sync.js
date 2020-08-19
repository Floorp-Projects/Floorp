/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Weave",
  "resource://services-sync/main.js"
);

const MIN_STATUS_ANIMATION_DURATION = 1600;

var gSync = {
  _initialized: false,
  // The last sync start time. Used to calculate the leftover animation time
  // once syncing completes (bug 1239042).
  _syncStartTime: 0,
  _syncAnimationTimer: 0,
  _obs: ["weave:engine:sync:finish", "quit-application", UIState.ON_UPDATE],

  get log() {
    if (!this._log) {
      const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
      let syncLog = Log.repository.getLogger("Sync.Browser");
      syncLog.manageLevelFromPref("services.sync.log.logger.browser");
      this._log = syncLog;
    }
    return this._log;
  },

  get fxaStrings() {
    delete this.fxaStrings;
    return (this.fxaStrings = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    ));
  },

  get syncStrings() {
    delete this.syncStrings;
    // XXXzpao these strings should probably be moved from /services to /browser... (bug 583381)
    //        but for now just make it work
    return (this.syncStrings = Services.strings.createBundle(
      "chrome://weave/locale/sync.properties"
    ));
  },

  // Returns true if FxA is configured, but the send tab targets list isn't
  // ready yet.
  get sendTabConfiguredAndLoading() {
    return (
      UIState.get().status == UIState.STATUS_SIGNED_IN &&
      !fxAccounts.device.recentDeviceList
    );
  },

  get isSignedIn() {
    return UIState.get().status == UIState.STATUS_SIGNED_IN;
  },

  getSendTabTargets() {
    // If sync is not enabled, then there's no point looking for sync clients.
    // If sync is simply not ready or hasn't yet synced the clients engine, we
    // just assume the fxa device doesn't have a sync record - in practice,
    // that just means we don't attempt to fall back to the "old" sendtab should
    // "new" sendtab fail.
    // We should just kill "old" sendtab now all our mobile browsers support
    // "new".
    let getClientRecord = () => undefined;
    if (UIState.get().syncEnabled && Weave.Service.clientsEngine) {
      getClientRecord = id =>
        Weave.Service.clientsEngine.getClientByFxaDeviceId(id);
    }
    let targets = [];
    if (!fxAccounts.device.recentDeviceList) {
      return targets;
    }
    for (let d of fxAccounts.device.recentDeviceList) {
      if (d.isCurrentDevice) {
        continue;
      }

      let clientRecord = getClientRecord(d.id);
      if (clientRecord || fxAccounts.commands.sendTab.isDeviceCompatible(d)) {
        targets.push({
          clientRecord,
          ...d,
        });
      }
    }
    return targets.sort((a, b) => a.name.localeCompare(b.name));
  },

  _generateNodeGetters() {
    for (let k of ["Status", "Avatar", "Label"]) {
      let prop = "appMenu" + k;
      let suffix = k.toLowerCase();
      delete this[prop];
      this.__defineGetter__(prop, function() {
        delete this[prop];
        return (this[prop] = document.getElementById("appMenu-fxa-" + suffix));
      });
    }
  },

  _definePrefGetters() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "UNSENDABLE_URL_REGEXP",
      "services.sync.engine.tabs.filteredUrls",
      null,
      null,
      rx => {
        try {
          return new RegExp(rx, "i");
        } catch (e) {
          Cu.reportError(
            `Failed to build url filter regexp for send tab: ${e}`
          );
          return null;
        }
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "FXA_ENABLED",
      "identity.fxaccounts.enabled"
    );
  },

  maybeUpdateUIState() {
    // Update the UI.
    if (UIState.isReady()) {
      const state = UIState.get();
      // If we are not configured, the UI is already in the right state when
      // we open the window. We can avoid a repaint.
      if (state.status != UIState.STATUS_NOT_CONFIGURED) {
        this.updateAllUI(state);
      }
    }
  },

  init() {
    if (this._initialized) {
      return;
    }

    this._definePrefGetters();

    if (!this.FXA_ENABLED) {
      this.onFxaDisabled();
      return;
    }

    MozXULElement.insertFTLIfNeeded("browser/sync.ftl");

    this._generateNodeGetters();

    // Label for the sync buttons.
    const appMenuLabel = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-label"
    );
    if (!appMenuLabel) {
      // We are in a window without our elements - just abort now, without
      // setting this._initialized, so we don't attempt to remove observers.
      return;
    }
    // We start with every menuitem hidden (except for the "setup sync" state),
    // so that we don't need to init the sync UI on windows like pageInfo.xhtml
    // (see bug 1384856).
    // maybeUpdateUIState() also optimizes for this - if we should be in the
    // "setup sync" state, that function assumes we are already in it and
    // doesn't re-initialize the UI elements.
    document.getElementById("sync-setup").hidden = false;
    PanelMultiView.getViewNode(
      document,
      "PanelUI-remotetabs-setupsync"
    ).hidden = false;

    for (let topic of this._obs) {
      Services.obs.addObserver(this, topic, true);
    }

    this.maybeUpdateUIState();

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
        this.updateFxAPanel(UIState.get());
        break;
    }
  },

  updateAllUI(state) {
    this.updatePanelPopup(state);
    this.updateState(state);
    this.updateSyncButtonsTooltip(state);
    this.updateSyncStatus(state);
    this.updateFxAPanel(state);
    // Ensure we have something in the device list in the background.
    this.ensureFxaDevices();
  },

  // Ensure we have *something* in `fxAccounts.device.recentDeviceList` as some
  // of our UI logic depends on it not being null. When FxA is notified of a
  // device change it will auto refresh `recentDeviceList`, and all UI which
  // shows the device list will start with `recentDeviceList`, but should also
  // force a refresh, both of which should mean in the worst-case, the UI is up
  // to date after a very short delay.
  async ensureFxaDevices(options) {
    if (UIState.get().status != UIState.STATUS_SIGNED_IN) {
      console.info("Skipping device list refresh; not signed in");
      return;
    }
    if (!fxAccounts.device.recentDeviceList) {
      if (await this.refreshFxaDevices()) {
        // Assuming we made the call successfully it should be impossible to end
        // up with a falsey recentDeviceList, so make noise if that's false.
        if (!fxAccounts.device.recentDeviceList) {
          console.warn("Refreshing device list didn't find any devices.");
        }
      }
    }
  },

  // Force a refresh of the fxa device list.  Note that while it's theoretically
  // OK to call `fxAccounts.device.refreshDeviceList` multiple times concurrently
  // and regularly, this call tells it to avoid those protections, so will always
  // hit the FxA servers - therefore, you should be very careful how often you
  // call this.
  // Returns Promise<bool> to indicate whether a refresh was actually done.
  async refreshFxaDevices() {
    if (UIState.get().status != UIState.STATUS_SIGNED_IN) {
      console.info("Skipping device list refresh; not signed in");
      return false;
    }
    try {
      // Do the actual refresh telling it to avoid the "flooding" protections.
      await fxAccounts.device.refreshDeviceList({ ignoreCached: true });
      return true;
    } catch (e) {
      this.log.error("Refreshing device list failed.", e);
      return false;
    }
  },

  updateSendToDeviceTitle() {
    let string = gBrowserBundle.GetStringFromName("sendTabsToDevice.label");
    let title = PluralForm.get(1, string).replace("#1", 1);
    if (gBrowser.selectedTab.multiselected) {
      let tabCount = gBrowser.selectedTabs.length;
      title = PluralForm.get(tabCount, string).replace("#1", tabCount);
    }

    PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-sendtab-button"
    ).setAttribute("label", title);
  },

  showSendToDeviceView(anchor) {
    PanelUI.showSubView("PanelUI-sendTabToDevice", anchor);
    let panelViewNode = document.getElementById("PanelUI-sendTabToDevice");
    this.populateSendTabToDevicesView(panelViewNode);
  },

  showSendToDeviceViewFromFxaMenu(anchor) {
    const { status } = UIState.get();
    if (status === UIState.STATUS_NOT_CONFIGURED) {
      PanelUI.showSubView("PanelUI-fxa-menu-sendtab-not-configured", anchor);
      return;
    }

    const targets = this.sendTabConfiguredAndLoading
      ? []
      : this.getSendTabTargets();
    if (!targets.length) {
      PanelUI.showSubView("PanelUI-fxa-menu-sendtab-no-devices", anchor);
      return;
    }

    this.showSendToDeviceView(anchor);
    this.emitFxaToolbarTelemetry("send_tab", anchor);
  },

  showRemoteTabsFromFxaMenu(panel) {
    PanelUI.showSubView("PanelUI-remotetabs", panel);
    this.emitFxaToolbarTelemetry("sync_tabs", panel);
  },

  showSidebarFromFxaMenu(panel) {
    SidebarUI.toggle("viewTabsSidebar");
    this.emitFxaToolbarTelemetry("sync_tabs_sidebar", panel);
  },

  populateSendTabToDevicesView(panelViewNode, reloadDevices = true) {
    let bodyNode = panelViewNode.querySelector(".panel-subview-body");
    let panelNode = panelViewNode.closest("panel");
    let browser = gBrowser.selectedBrowser;
    let url = browser.currentURI.spec;
    let title = browser.contentTitle;
    let multiselected = gBrowser.selectedTab.multiselected;

    // This is on top because it also clears the device list between state
    // changes.
    this.populateSendTabToDevicesMenu(
      bodyNode,
      url,
      title,
      multiselected,
      (clientId, name, clientType, lastModified) => {
        if (!name) {
          return document.createXULElement("toolbarseparator");
        }
        let item = document.createXULElement("toolbarbutton");
        item.classList.add("pageAction-sendToDevice-device", "subviewbutton");
        if (clientId) {
          item.classList.add("subviewbutton-iconic");
          if (lastModified) {
            item.setAttribute(
              "tooltiptext",
              gSync.formatLastSyncDate(lastModified)
            );
          }
        }

        item.addEventListener("command", event => {
          if (panelNode) {
            PanelMultiView.hidePopup(panelNode);
          }
        });
        return item;
      }
    );

    bodyNode.removeAttribute("state");
    // If the app just started, we won't have fetched the device list yet. Sync
    // does this automatically ~10 sec after startup, but there's no trigger for
    // this if we're signed in to FxA, but not Sync.
    if (gSync.sendTabConfiguredAndLoading) {
      bodyNode.setAttribute("state", "notready");
    }
    if (reloadDevices) {
      // We will only pick up new Fennec clients if we sync the clients engine,
      // but all other send-tab targets can be identified purely from the fxa
      // device list. Syncing the clients engine doesn't force a refresh of the
      // fxa list, and it seems overkill to force *both* a clients engine sync
      // and an fxa device list refresh, especially given (a) the clients engine
      // will sync by itself every 10 minutes and (b) Fennec is (at time of
      // writing) about to be replaced by Fenix.
      // So we suck up the fact that new Fennec clients may not appear for 10
      // minutes and don't bother syncing the clients engine.

      // Force a refresh of the fxa device list in case the user connected a new
      // device, and is waiting for it to show up.
      this.refreshFxaDevices().then(_ => {
        if (!window.closed) {
          this.populateSendTabToDevicesView(panelViewNode, false);
        }
      });
    }
  },

  toggleAccountPanel(
    viewId,
    anchor = document.getElementById("fxa-toolbar-menu-button"),
    aEvent
  ) {
    // Don't show the panel if the window is in customization mode.
    if (document.documentElement.hasAttribute("customizing")) {
      return;
    }

    if (
      (aEvent.type == "mousedown" && aEvent.button != 0) ||
      (aEvent.type == "keypress" &&
        aEvent.charCode != KeyEvent.DOM_VK_SPACE &&
        aEvent.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return;
    }

    if (!gFxaToolbarAccessed) {
      Services.prefs.setBoolPref("identity.fxaccounts.toolbar.accessed", true);
    }

    this.enableSendTabIfValidTab();

    if (anchor.getAttribute("open") == "true") {
      PanelUI.hide();
    } else {
      this.emitFxaToolbarTelemetry("toolbar_icon", anchor);
      PanelUI.showSubView(viewId, anchor, aEvent);
    }
  },

  updateFxAPanel(state = {}) {
    const mainWindowEl = document.documentElement;

    // The Firefox Account toolbar currently handles 3 different states for
    // users. The default `not_configured` state shows an empty avatar, `unverified`
    // state shows an avatar with an email icon, `login-failed` state shows an avatar
    // with a danger icon and the `verified` state will show the users
    // custom profile image or a filled avatar.
    let stateValue = "not_configured";

    const menuHeaderTitleEl = PanelMultiView.getViewNode(
      document,
      "fxa-menu-header-title"
    );
    const menuHeaderDescriptionEl = PanelMultiView.getViewNode(
      document,
      "fxa-menu-header-description"
    );

    const cadButtonEl = PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-connect-device-button"
    );

    const syncSetupButtonEl = PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-setup-sync-button"
    );

    const syncPrefsButtonEl = PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-sync-prefs-button"
    );

    const syncNowButtonEl = PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-syncnow-button"
    );
    const fxaMenuPanel = PanelMultiView.getViewNode(document, "PanelUI-fxa");

    const fxaMenuAccountButtonEl = PanelMultiView.getViewNode(
      document,
      "fxa-manage-account-button"
    );

    let headerTitle = menuHeaderTitleEl.getAttribute("defaultLabel");
    let headerDescription = menuHeaderDescriptionEl.getAttribute(
      "defaultLabel"
    );

    const appMenuFxAButtonEl = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-label"
    );

    let panelTitle = this.fxaStrings.GetStringFromName("account.title");

    fxaMenuPanel.removeAttribute("title");
    cadButtonEl.setAttribute("disabled", true);
    syncNowButtonEl.setAttribute("hidden", true);
    fxaMenuAccountButtonEl.classList.remove("subviewbutton-nav");
    fxaMenuAccountButtonEl.removeAttribute("closemenu");
    syncPrefsButtonEl.setAttribute("hidden", true);
    syncSetupButtonEl.removeAttribute("hidden");

    if (state.status === UIState.STATUS_NOT_CONFIGURED) {
      mainWindowEl.style.removeProperty("--avatar-image-url");
    } else if (state.status === UIState.STATUS_LOGIN_FAILED) {
      stateValue = "login-failed";
      headerTitle = this.fxaStrings.GetStringFromName("account.reconnectToFxA");
      headerDescription = state.email;
      mainWindowEl.style.removeProperty("--avatar-image-url");
    } else if (state.status === UIState.STATUS_NOT_VERIFIED) {
      stateValue = "unverified";
      headerTitle = this.fxaStrings.GetStringFromName(
        "account.finishAccountSetup"
      );
      headerDescription = state.email;
    } else if (state.status === UIState.STATUS_SIGNED_IN) {
      stateValue = "signedin";
      if (state.avatarURL && !state.avatarIsDefault) {
        // The user has specified a custom avatar, attempt to load the image on all the menu buttons.
        const bgImage = `url("${state.avatarURL}")`;
        let img = new Image();
        img.onload = () => {
          // If the image has successfully loaded, update the menu buttons else
          // we will use the default avatar image.
          mainWindowEl.style.setProperty("--avatar-image-url", bgImage);
        };
        img.onerror = () => {
          // If the image failed to load, remove the property and default
          // to standard avatar.
          mainWindowEl.style.removeProperty("--avatar-image-url");
        };
        img.src = state.avatarURL;
      } else {
        mainWindowEl.style.removeProperty("--avatar-image-url");
      }

      cadButtonEl.removeAttribute("disabled");

      if (state.syncEnabled) {
        syncNowButtonEl.removeAttribute("hidden");
        syncPrefsButtonEl.removeAttribute("hidden");
        syncSetupButtonEl.setAttribute("hidden", true);
      }

      fxaMenuAccountButtonEl.classList.add("subviewbutton-nav");
      fxaMenuAccountButtonEl.setAttribute("closemenu", "none");

      headerTitle = state.email;
      headerDescription = this.fxaStrings.GetStringFromName(
        "account.accountSettings"
      );

      panelTitle = state.displayName ? state.displayName : panelTitle;
    }
    mainWindowEl.setAttribute("fxastatus", stateValue);

    menuHeaderTitleEl.value = headerTitle;
    menuHeaderDescriptionEl.value = headerDescription;
    appMenuFxAButtonEl.setAttribute("label", headerTitle);

    fxaMenuPanel.setAttribute("title", panelTitle);
  },

  enableSendTabIfValidTab() {
    // All tabs selected must be sendable for the Send Tab button to be enabled
    // on the FxA menu.
    let canSendAllURIs = gBrowser.selectedTabs.every(t =>
      this.isSendableURI(t.linkedBrowser.currentURI.spec)
    );

    if (canSendAllURIs) {
      PanelMultiView.getViewNode(
        document,
        "PanelUI-fxa-menu-sendtab-button"
      ).removeAttribute("disabled");
    } else {
      PanelMultiView.getViewNode(
        document,
        "PanelUI-fxa-menu-sendtab-button"
      ).setAttribute("disabled", true);
    }
  },

  emitFxaToolbarTelemetry(type, panel) {
    if (UIState.isReady() && panel) {
      const state = UIState.get();
      const hasAvatar = state.avatarURL && !state.avatarIsDefault;
      let extraOptions = {
        fxa_status: state.status,
        fxa_avatar: hasAvatar ? "true" : "false",
      };

      // When the fxa avatar panel is within the Firefox app menu,
      // we emit different telemetry.
      let eventName = "fxa_avatar_menu";
      if (this.isPanelInsideAppMenu(panel)) {
        eventName = "fxa_app_menu";
      }

      Services.telemetry.recordEvent(
        eventName,
        "click",
        type,
        null,
        extraOptions
      );
    }
  },

  isPanelInsideAppMenu(panel = undefined) {
    const appMenuPanel = document.getElementById("appMenu-popup");
    if (panel && appMenuPanel.contains(panel)) {
      return true;
    }
    return false;
  },

  updatePanelPopup(state) {
    const appMenuStatus = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-status"
    );
    const appMenuLabel = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-label"
    );
    const appMenuAvatar = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-avatar"
    );

    let defaultLabel = appMenuStatus.getAttribute("defaultlabel");
    const status = state.status;
    // Reset the status bar to its original state.
    appMenuLabel.setAttribute("label", defaultLabel);
    appMenuStatus.removeAttribute("fxastatus");
    appMenuAvatar.style.removeProperty("list-style-image");
    appMenuLabel.classList.remove("subviewbutton-nav");

    if (status == UIState.STATUS_NOT_CONFIGURED) {
      return;
    }

    // At this point we consider sync to be configured (but still can be in an error state).
    if (status == UIState.STATUS_LOGIN_FAILED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName(
        "reconnectDescription",
        [state.email]
      );
      let errorLabel = appMenuStatus.getAttribute("errorlabel");
      appMenuStatus.setAttribute("fxastatus", "login-failed");
      appMenuLabel.setAttribute("label", errorLabel);
      appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      return;
    } else if (status == UIState.STATUS_NOT_VERIFIED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName(
        "verifyDescription",
        [state.email]
      );
      let unverifiedLabel = appMenuStatus.getAttribute("unverifiedlabel");
      appMenuStatus.setAttribute("fxastatus", "unverified");
      appMenuLabel.setAttribute("label", unverifiedLabel);
      appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      return;
    }

    // At this point we consider sync to be logged-in.
    appMenuStatus.setAttribute("fxastatus", "signedin");
    appMenuLabel.setAttribute("label", state.displayName || state.email);
    appMenuLabel.classList.add("subviewbutton-nav");
    appMenuStatus.removeAttribute("tooltiptext");
  },

  updateState(state) {
    for (let [shown, menuId, boxId] of [
      [
        state.status == UIState.STATUS_NOT_CONFIGURED,
        "sync-setup",
        "PanelUI-remotetabs-setupsync",
      ],
      [
        state.status == UIState.STATUS_SIGNED_IN && !state.syncEnabled,
        "sync-enable",
        "PanelUI-remotetabs-syncdisabled",
      ],
      [
        state.status == UIState.STATUS_LOGIN_FAILED,
        "sync-reauthitem",
        "PanelUI-remotetabs-reauthsync",
      ],
      [
        state.status == UIState.STATUS_NOT_VERIFIED,
        "sync-unverifieditem",
        "PanelUI-remotetabs-unverified",
      ],
      [
        state.status == UIState.STATUS_SIGNED_IN && state.syncEnabled,
        "sync-syncnowitem",
        "PanelUI-remotetabs-main",
      ],
    ]) {
      document.getElementById(menuId).hidden = PanelMultiView.getViewNode(
        document,
        boxId
      ).hidden = !shown;
    }
  },

  updateSyncStatus(state) {
    let syncNow =
      document.querySelector(".syncNowBtn") ||
      document
        .getElementById("appMenu-viewCache")
        .content.querySelector(".syncNowBtn");
    const syncingUI = syncNow.getAttribute("syncstatus") == "active";
    if (state.syncing != syncingUI) {
      // Do we need to update the UI?
      state.syncing ? this.onActivityStart() : this.onActivityStop();
    }
  },

  async openSignInAgainPage(entryPoint) {
    const url = await FxAccounts.config.promiseForceSigninURI(entryPoint);
    switchToTabHavingURI(url, true, {
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  async openDevicesManagementPage(entryPoint) {
    let url = await FxAccounts.config.promiseManageDevicesURI(entryPoint);
    switchToTabHavingURI(url, true, {
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  async openConnectAnotherDevice(entryPoint) {
    const url = await FxAccounts.config.promiseConnectDeviceURI(entryPoint);
    openTrustedLinkIn(url, "tab");
  },

  async openConnectAnotherDeviceFromFxaMenu(panel = undefined) {
    this.emitFxaToolbarTelemetry("cad", panel);
    let entryPoint = "fxa_discoverability_native";
    if (this.isPanelInsideAppMenu(panel)) {
      entryPoint = "fxa_app_menu";
    }
    this.openConnectAnotherDevice(entryPoint);
  },

  openSendToDevicePromo() {
    const url = Services.urlFormatter.formatURLPref(
      "identity.sendtabpromo.url"
    );
    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  async clickFxAMenuHeaderButton(panel = undefined) {
    // Depending on the current logged in state of a user,
    // clicking the FxA header will either open
    // a sign-in page, account management page, or sync
    // preferences page.
    const { status } = UIState.get();
    switch (status) {
      case UIState.STATUS_NOT_CONFIGURED:
        this.openFxAEmailFirstPageFromFxaMenu(panel);
        break;
      case UIState.STATUS_LOGIN_FAILED:
      case UIState.STATUS_NOT_VERIFIED:
        this.openPrefsFromFxaMenu("sync_settings", panel);
        break;
      case UIState.STATUS_SIGNED_IN:
        PanelUI.showSubView("PanelUI-fxa-menu-account-panel", panel);
    }
  },

  async openFxAEmailFirstPage(entryPoint) {
    const url = await FxAccounts.config.promiseConnectAccountURI(entryPoint);
    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  async openFxAEmailFirstPageFromFxaMenu(panel = undefined) {
    this.emitFxaToolbarTelemetry("login", panel);
    let entryPoint = "fxa_discoverability_native";
    if (this.isPanelInsideAppMenu(panel)) {
      entryPoint = "fxa_app_menu";
    }
    this.openFxAEmailFirstPage(entryPoint);
  },

  async openFxAManagePage(entryPoint) {
    const url = await FxAccounts.config.promiseManageURI(entryPoint);
    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  async openFxAManagePageFromFxaMenu(panel = undefined) {
    this.emitFxaToolbarTelemetry("account_settings", panel);
    let entryPoint = "fxa_discoverability_native";
    if (this.isPanelInsideAppMenu(panel)) {
      entryPoint = "fxa_app_menu";
    }
    this.openFxAManagePage(entryPoint);
  },

  async openSendFromFxaMenu(panel) {
    this.emitFxaToolbarTelemetry("open_send", panel);
    this.launchFxaService(gFxaSendLoginUrl);
  },

  async openMonitorFromFxaMenu(panel) {
    this.emitFxaToolbarTelemetry("open_monitor", panel);
    this.launchFxaService(gFxaMonitorLoginUrl);
  },

  launchFxaService(serviceUrl, panel) {
    let entryPoint = "fxa_discoverability_native";
    if (this.isPanelInsideAppMenu(panel)) {
      entryPoint = "fxa_app_menu";
    }

    const url = new URL(serviceUrl);
    url.searchParams.set("utm_source", "fxa-toolbar");
    url.searchParams.set("utm_medium", "referral");
    url.searchParams.set("entrypoint", entryPoint);

    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN) {
      url.searchParams.set("email", state.email);
    }

    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  // Returns true if we managed to send the tab to any targets, false otherwise.
  async sendTabToDevice(url, targets, title) {
    const fxaCommandsDevices = [];
    const oldSendTabClients = [];
    for (const target of targets) {
      if (fxAccounts.commands.sendTab.isDeviceCompatible(target)) {
        fxaCommandsDevices.push(target);
      } else if (target.clientRecord) {
        oldSendTabClients.push(target.clientRecord);
      } else {
        this.log.error(`Target ${target.id} unsuitable for send tab.`);
      }
    }
    // If a master-password is enabled then it must be unlocked so FxA can get
    // the encryption keys from the login manager. (If we end up using the "sync"
    // fallback that would end up prompting by itself, but the FxA command route
    // will not) - so force that here.
    let cryptoSDR = Cc["@mozilla.org/login-manager/crypto/SDR;1"].getService(
      Ci.nsILoginManagerCrypto
    );
    if (!cryptoSDR.isLoggedIn) {
      if (cryptoSDR.uiBusy) {
        this.log.info("Master password UI is busy - not sending the tabs");
        return false;
      }
      try {
        cryptoSDR.encrypt("bacon"); // forces the mp prompt.
      } catch (e) {
        this.log.info(
          "Master password remains unlocked - not sending the tabs"
        );
        return false;
      }
    }
    let numFailed = 0;
    if (fxaCommandsDevices.length) {
      this.log.info(
        `Sending a tab to ${fxaCommandsDevices
          .map(d => d.id)
          .join(", ")} using FxA commands.`
      );
      const report = await fxAccounts.commands.sendTab.send(
        fxaCommandsDevices,
        { url, title }
      );
      for (let { device, error } of report.failed) {
        this.log.error(
          `Failed to send a tab with FxA commands for ${device.id}.`,
          error
        );
        numFailed++;
      }
    }
    for (let client of oldSendTabClients) {
      try {
        this.log.info(`Sending a tab to ${client.id} using Sync.`);
        await Weave.Service.clientsEngine.sendURIToClientForDisplay(
          url,
          client.id,
          title
        );
      } catch (e) {
        numFailed++;
        this.log.error("Could not send tab to device.", e);
      }
    }
    return numFailed < targets.length; // Good enough.
  },

  populateSendTabToDevicesMenu(
    devicesPopup,
    url,
    title,
    multiselected,
    createDeviceNodeFn
  ) {
    if (!createDeviceNodeFn) {
      createDeviceNodeFn = (targetId, name, targetType, lastModified) => {
        let eltName = name ? "menuitem" : "menuseparator";
        return document.createXULElement(eltName);
      };
    }

    // remove existing menu items
    for (let i = devicesPopup.children.length - 1; i >= 0; --i) {
      let child = devicesPopup.children[i];
      if (child.classList.contains("sync-menuitem")) {
        child.remove();
      }
    }

    if (gSync.sendTabConfiguredAndLoading) {
      // We can only be in this case in the page action menu.
      return;
    }

    const fragment = document.createDocumentFragment();

    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN) {
      const targets = this.getSendTabTargets();
      if (targets.length) {
        this._appendSendTabDeviceList(
          targets,
          fragment,
          createDeviceNodeFn,
          url,
          title,
          multiselected
        );
      } else {
        this._appendSendTabSingleDevice(fragment, createDeviceNodeFn);
      }
    } else if (
      state.status == UIState.STATUS_NOT_VERIFIED ||
      state.status == UIState.STATUS_LOGIN_FAILED
    ) {
      this._appendSendTabVerify(fragment, createDeviceNodeFn);
    } /* status is STATUS_NOT_CONFIGURED */ else {
      this._appendSendTabUnconfigured(fragment, createDeviceNodeFn);
    }

    devicesPopup.appendChild(fragment);
  },

  _appendSendTabDeviceList(
    targets,
    fragment,
    createDeviceNodeFn,
    url,
    title,
    multiselected
  ) {
    let tabsToSend = multiselected
      ? gBrowser.selectedTabs.map(t => {
          return {
            url: t.linkedBrowser.currentURI.spec,
            title: t.linkedBrowser.contentTitle,
          };
        })
      : [{ url, title }];

    const send = to => {
      Promise.all(
        tabsToSend.map(t =>
          // sendTabToDevice does not reject.
          this.sendTabToDevice(t.url, to, t.title)
        )
      ).then(results => {
        if (results.includes(true)) {
          let action = PageActions.actionForID("sendToDevice");
          showBrowserPageActionFeedback(action);
        }
        fxAccounts.flushLogFile();
      });
    };
    const onSendAllCommand = event => {
      send(targets);
    };
    const onTargetDeviceCommand = event => {
      const targetId = event.target.getAttribute("clientId");
      const target = targets.find(t => t.id == targetId);
      send([target]);
    };

    function addTargetDevice(targetId, name, targetType, lastModified) {
      const targetDevice = createDeviceNodeFn(
        targetId,
        name,
        targetType,
        lastModified
      );
      targetDevice.addEventListener(
        "command",
        targetId ? onTargetDeviceCommand : onSendAllCommand,
        true
      );
      targetDevice.classList.add("sync-menuitem", "sendtab-target");
      targetDevice.setAttribute("clientId", targetId);
      targetDevice.setAttribute("clientType", targetType);
      targetDevice.setAttribute("label", name);
      fragment.appendChild(targetDevice);
    }

    for (let target of targets) {
      let type, lastModified;
      if (target.clientRecord) {
        type = Weave.Service.clientsEngine.getClientType(
          target.clientRecord.id
        );
        lastModified = new Date(target.clientRecord.serverLastModified * 1000);
      } else {
        // For phones, FxA uses "mobile" and Sync clients uses "phone".
        type = target.type == "mobile" ? "phone" : target.type;
        lastModified = target.lastAccessTime
          ? new Date(target.lastAccessTime)
          : null;
      }
      addTargetDevice(target.id, target.name, type, lastModified);
    }

    // "Send to All Devices" menu item
    if (targets.length > 1) {
      const separator = createDeviceNodeFn();
      separator.classList.add("sync-menuitem");
      fragment.appendChild(separator);
      const allDevicesLabel = this.fxaStrings.GetStringFromName(
        "sendToAllDevices.menuitem"
      );
      addTargetDevice("", allDevicesLabel, "");
    }
  },

  _appendSendTabSingleDevice(fragment, createDeviceNodeFn) {
    const noDevices = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.singledevice.status"
    );
    const learnMore = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.singledevice"
    );
    const connectDevice = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.connectdevice"
    );
    const actions = [
      {
        label: connectDevice,
        command: () => this.openConnectAnotherDevice("sendtab"),
      },
      { label: learnMore, command: () => this.openSendToDevicePromo() },
    ];
    this._appendSendTabInfoItems(
      fragment,
      createDeviceNodeFn,
      noDevices,
      actions
    );
  },

  _appendSendTabVerify(fragment, createDeviceNodeFn) {
    const notVerified = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.verify.status"
    );
    const verifyAccount = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.verify"
    );
    const actions = [
      { label: verifyAccount, command: () => this.openPrefs("sendtab") },
    ];
    this._appendSendTabInfoItems(
      fragment,
      createDeviceNodeFn,
      notVerified,
      actions
    );
  },

  _appendSendTabUnconfigured(fragment, createDeviceNodeFn) {
    const brandProductName = gBrandBundle.GetStringFromName("brandProductName");
    const notConnected = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.unconfigured.label2"
    );
    const learnMore = this.fxaStrings.GetStringFromName(
      "sendTabToDevice.unconfigured"
    );
    const actions = [
      { label: learnMore, command: () => this.openSendToDevicePromo() },
    ];
    this._appendSendTabInfoItems(
      fragment,
      createDeviceNodeFn,
      notConnected,
      actions
    );

    // Now add a 'sign in to Firefox' item above the 'learn more' item.
    const signInToFxA = this.fxaStrings.formatStringFromName(
      "sendTabToDevice.signintofxa",
      [brandProductName]
    );
    let signInItem = createDeviceNodeFn(null, signInToFxA, null);
    signInItem.classList.add("sync-menuitem");
    signInItem.setAttribute("label", signInToFxA);
    // Show an icon if opened in the page action panel:
    if (signInItem.classList.contains("subviewbutton")) {
      signInItem.classList.add("subviewbutton-iconic", "signintosync");
    }
    signInItem.addEventListener("command", () => {
      this.openPrefs("sendtab");
    });
    fragment.insertBefore(signInItem, fragment.lastElementChild);
  },

  _appendSendTabInfoItems(fragment, createDeviceNodeFn, statusLabel, actions) {
    const status = createDeviceNodeFn(null, statusLabel, null);
    status.setAttribute("label", statusLabel);
    status.setAttribute("disabled", true);
    status.classList.add("sync-menuitem");
    fragment.appendChild(status);

    const separator = createDeviceNodeFn(null, null, null);
    separator.classList.add("sync-menuitem");
    fragment.appendChild(separator);

    for (let { label, command } of actions) {
      const actionItem = createDeviceNodeFn(null, label, null);
      actionItem.addEventListener("command", command, true);
      actionItem.classList.add("sync-menuitem");
      actionItem.setAttribute("label", label);
      fragment.appendChild(actionItem);
    }
  },

  isSendableURI(aURISpec) {
    if (!aURISpec) {
      return false;
    }
    // Disallow sending tabs with more than 65535 characters.
    if (aURISpec.length > 65535) {
      return false;
    }
    if (this.UNSENDABLE_URL_REGEXP) {
      return !this.UNSENDABLE_URL_REGEXP.test(aURISpec);
    }
    // The preference has been removed, or is an invalid regexp, so we treat it
    // as a valid URI. We've already logged an error when trying to construct
    // the regexp, and the more problematic case is the length, which we've
    // already addressed.
    return true;
  },

  // "Send Tab to Device" menu item
  updateTabContextMenu(aPopupMenu, aTargetTab) {
    // We may get here before initialisation. This situation
    // can lead to a empty label for 'Send To Device' Menu.
    this.init();

    if (!this.FXA_ENABLED) {
      // These items are hidden in onFxaDisabled(). No need to do anything.
      return;
    }
    let hasASendableURI = false;
    for (let tab of aTargetTab.multiselected
      ? gBrowser.selectedTabs
      : [aTargetTab]) {
      if (this.isSendableURI(tab.linkedBrowser.currentURI.spec)) {
        hasASendableURI = true;
        break;
      }
    }
    const enabled = !this.sendTabConfiguredAndLoading && hasASendableURI;

    let sendTabsToDevice = document.getElementById("context_sendTabToDevice");
    sendTabsToDevice.disabled = !enabled;

    let tabCount = aTargetTab.multiselected
      ? gBrowser.multiSelectedTabsCount
      : 1;
    sendTabsToDevice.label = PluralForm.get(
      tabCount,
      gNavigatorBundle.getString("sendTabsToDevice.label")
    ).replace("#1", tabCount.toLocaleString());
    sendTabsToDevice.accessKey = gNavigatorBundle.getString(
      "sendTabsToDevice.accesskey"
    );
  },

  // "Send Page to Device" and "Send Link to Device" menu items
  updateContentContextMenu(contextMenu) {
    if (!this.FXA_ENABLED) {
      // These items are hidden by default. No need to do anything.
      return;
    }
    // showSendLink and showSendPage are mutually exclusive
    const showSendLink =
      contextMenu.onSaveableLink || contextMenu.onPlainTextLink;
    const showSendPage =
      !showSendLink &&
      !(
        contextMenu.isContentSelected ||
        contextMenu.onImage ||
        contextMenu.onCanvas ||
        contextMenu.onVideo ||
        contextMenu.onAudio ||
        contextMenu.onLink ||
        contextMenu.onTextInput
      );

    // Avoids double separator on images with links.
    const hideSeparator =
      contextMenu.isContentSelected &&
      contextMenu.onLink &&
      contextMenu.onImage;
    [
      "context-sendpagetodevice",
      ...(hideSeparator ? [] : ["context-sep-sendpagetodevice"]),
    ].forEach(id => contextMenu.showItem(id, showSendPage));
    [
      "context-sendlinktodevice",
      ...(hideSeparator ? [] : ["context-sep-sendlinktodevice"]),
    ].forEach(id => contextMenu.showItem(id, showSendLink));

    if (!showSendLink && !showSendPage) {
      return;
    }

    const targetURI = showSendLink
      ? contextMenu.linkURL
      : contextMenu.browser.currentURI.spec;
    const enabled =
      !this.sendTabConfiguredAndLoading && this.isSendableURI(targetURI);
    contextMenu.setItemAttr(
      showSendPage ? "context-sendpagetodevice" : "context-sendlinktodevice",
      "disabled",
      !enabled || null
    );
  },

  // Functions called by observers
  onActivityStart() {
    clearTimeout(this._syncAnimationTimer);
    this._syncStartTime = Date.now();

    document.querySelectorAll(".syncNowBtn").forEach(el => {
      el.setAttribute("syncstatus", "active");
      el.setAttribute("disabled", "true");
      document.l10n.setAttributes(el, el.getAttribute("syncinglabel"));
    });

    document
      .getElementById("appMenu-viewCache")
      .content.querySelectorAll(".syncNowBtn")
      .forEach(el => {
        el.setAttribute("syncstatus", "active");
        el.setAttribute("disabled", "true");
        document.l10n.setAttributes(el, el.getAttribute("syncinglabel"));
      });
  },

  _onActivityStop() {
    if (!gBrowser) {
      return;
    }

    document.querySelectorAll(".syncNowBtn").forEach(el => {
      el.removeAttribute("syncstatus");
      el.removeAttribute("disabled");
      document.l10n.setAttributes(el, "fxa-toolbar-sync-now");
    });

    document
      .getElementById("appMenu-viewCache")
      .content.querySelectorAll(".syncNowBtn")
      .forEach(el => {
        el.removeAttribute("syncstatus");
        el.removeAttribute("disabled");
        document.l10n.setAttributes(el, "fxa-toolbar-sync-now");
      });

    Services.obs.notifyObservers(null, "test:browser-sync:activity-stop");
  },

  onActivityStop() {
    let now = Date.now();
    let syncDuration = now - this._syncStartTime;

    if (syncDuration < MIN_STATUS_ANIMATION_DURATION) {
      let animationTime = MIN_STATUS_ANIMATION_DURATION - syncDuration;
      clearTimeout(this._syncAnimationTimer);
      this._syncAnimationTimer = setTimeout(
        () => this._onActivityStop(),
        animationTime
      );
    } else {
      this._onActivityStop();
    }
  },

  // Disconnect from sync, and optionally disconnect from the FxA account.
  // Returns true if the disconnection happened (ie, if the user didn't decline
  // when asked to confirm)
  async disconnect({ confirm = true, disconnectAccount = true } = {}) {
    if (disconnectAccount) {
      let deleteLocalData = false;
      if (confirm) {
        let options = await this._confirmFxaAndSyncDisconnect();
        if (!options.userConfirmedDisconnect) {
          return false;
        }
        deleteLocalData = options.deleteLocalData;
      }
      return this._disconnectFxaAndSync(deleteLocalData);
    }

    if (confirm && !(await this._confirmSyncDisconnect())) {
      return false;
    }
    return this._disconnectSync();
  },

  // Prompt the user to confirm disconnect from FxA and sync with the option
  // to delete syncable data from the device.
  async _confirmFxaAndSyncDisconnect() {
    let options = {
      userConfirmedDisconnect: false,
    };

    window.openDialog(
      "chrome://browser/content/browser-fxaSignout.xhtml",
      "_blank",
      "chrome,modal,centerscreen,resizable=no",
      { hideDeleteDataOption: !UIState.get().syncEnabled },
      options
    );

    return options;
  },

  async _disconnectFxaAndSync(deleteLocalData) {
    const { SyncDisconnect } = ChromeUtils.import(
      "resource://services-sync/SyncDisconnect.jsm"
    );
    // Record telemetry.
    await fxAccounts.telemetry.recordDisconnection(null, "ui");

    await SyncDisconnect.disconnect(deleteLocalData).catch(e => {
      console.error("Failed to disconnect.", e);
    });

    return true;
  },

  // Prompt the user to confirm disconnect from sync. In this case the data
  // on the device is not deleted.
  async _confirmSyncDisconnect() {
    const l10nPrefix = "sync-disconnect-dialog";

    const [title, body, button] = await document.l10n.formatValues([
      { id: `${l10nPrefix}-title` },
      { id: `${l10nPrefix}-body` },
      { id: "sync-disconnect-dialog-button" },
    ]);

    const flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1;

    // buttonPressed will be 0 for disconnect, 1 for cancel.
    const buttonPressed = Services.prompt.confirmEx(
      window,
      title,
      body,
      flags,
      button,
      null,
      null,
      null,
      {}
    );
    return buttonPressed == 0;
  },

  async _disconnectSync() {
    await fxAccounts.telemetry.recordDisconnection("sync", "ui");

    await Weave.Service.promiseInitialized;
    await Weave.Service.startOver();

    return true;
  },

  // doSync forces a sync - it *does not* return a promise as it is called
  // via the various UI components.
  doSync() {
    if (!UIState.isReady()) {
      return;
    }
    // Note we don't bother checking if sync is actually enabled - none of the
    // UI which calls this function should be visible in that case.
    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN) {
      this.updateSyncStatus({ syncing: true });
      Services.tm.dispatchToMainThread(() => {
        // We are pretty confident that push helps us pick up all FxA commands,
        // but some users might have issues with push, so let's unblock them
        // by fetching the missed FxA commands on manual sync.
        fxAccounts.commands.pollDeviceCommands().catch(e => {
          this.log.error("Fetching missed remote commands failed.", e);
        });
        Weave.Service.sync();
      });
    }
  },

  doSyncFromFxaMenu(panel) {
    this.doSync();
    this.emitFxaToolbarTelemetry("sync_now", panel);
  },

  openPrefs(entryPoint = "syncbutton", origin = undefined) {
    window.openPreferences("paneSync", {
      origin,
      urlParams: { entrypoint: entryPoint },
    });
  },

  openPrefsFromFxaMenu(type, panel) {
    this.emitFxaToolbarTelemetry(type, panel);
    let entryPoint = "fxa_discoverability_native";
    if (this.isPanelInsideAppMenu(panel)) {
      entryPoint = "fxa_app_menu";
    }
    this.openPrefs(entryPoint);
  },

  openSyncedTabsPanel() {
    let placement = CustomizableUI.getPlacementOfWidget("sync-button");
    let area = placement && placement.area;
    let anchor =
      document.getElementById("sync-button") ||
      document.getElementById("PanelUI-menu-button");
    if (area == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
      // The button is in the overflow panel, so we need to show the panel,
      // then show our subview.
      let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
      navbar.overflowable.show().then(() => {
        PanelUI.showSubView("PanelUI-remotetabs", anchor);
      }, Cu.reportError);
    } else {
      // It is placed somewhere else - just try and show it.
      PanelUI.showSubView("PanelUI-remotetabs", anchor);
    }
  },

  refreshSyncButtonsTooltip() {
    const state = UIState.get();
    this.updateSyncButtonsTooltip(state);
  },

  /* Update the tooltip for the sync icon in the main menu and in Synced Tabs.
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
      tooltiptext = this.fxaStrings.formatStringFromName("verifyDescription", [
        state.email,
      ]);
    } else if (status == UIState.STATUS_NOT_CONFIGURED) {
      // "needs setup".
      tooltiptext = this.syncStrings.GetStringFromName(
        "signInToSync.description"
      );
    } else if (status == UIState.STATUS_LOGIN_FAILED) {
      // "need to reconnect/re-enter your password"
      tooltiptext = this.fxaStrings.formatStringFromName(
        "reconnectDescription",
        [state.email]
      );
    } else {
      // Sync appears configured - format the "last synced at" time.
      tooltiptext = this.formatLastSyncDate(state.lastSync);
    }

    document.querySelectorAll(".syncNowBtn").forEach(el => {
      if (tooltiptext) {
        el.setAttribute("tooltiptext", tooltiptext);
      } else {
        el.removeAttribute("tooltiptext");
      }
    });

    document
      .getElementById("appMenu-viewCache")
      .content.querySelectorAll(".syncNowBtn")
      .forEach(el => {
        if (tooltiptext) {
          el.setAttribute("tooltiptext", tooltiptext);
        } else {
          el.removeAttribute("tooltiptext");
        }
      });
  },

  get relativeTimeFormat() {
    delete this.relativeTimeFormat;
    return (this.relativeTimeFormat = new Services.intl.RelativeTimeFormat(
      undefined,
      { style: "long" }
    ));
  },

  formatLastSyncDate(date) {
    if (!date) {
      // Date can be null before the first sync!
      return null;
    }
    try {
      const relativeDateStr = this.relativeTimeFormat.formatBestUnit(date);
      return this.syncStrings.formatStringFromName("lastSync2.label", [
        relativeDateStr,
      ]);
    } catch (ex) {
      // shouldn't happen, but one client having an invalid date shouldn't
      // break the entire feature.
      this.log.warn("failed to format lastSync time", date, ex);
      return null;
    }
  },

  onClientsSynced() {
    // Note that this element is only shown if Sync is enabled.
    let element = PanelMultiView.getViewNode(
      document,
      "PanelUI-remotetabs-main"
    );
    if (element) {
      if (Weave.Service.clientsEngine.stats.numClients > 1) {
        element.setAttribute("devices-status", "multi");
      } else {
        element.setAttribute("devices-status", "single");
      }
    }
  },

  onFxaDisabled() {
    const toHide = [...document.querySelectorAll(".sync-ui-item")];
    for (const item of toHide) {
      item.hidden = true;
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};
