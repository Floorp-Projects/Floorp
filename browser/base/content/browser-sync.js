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

XPCOMUtils.defineLazyModuleGetters(this, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

const MIN_STATUS_ANIMATION_DURATION = 1600;

this.SyncedTabsPanelList = class SyncedTabsPanelList {
  static sRemoteTabsDeckIndices = {
    DECKINDEX_TABS: 0,
    DECKINDEX_FETCHING: 1,
    DECKINDEX_TABSDISABLED: 2,
    DECKINDEX_NOCLIENTS: 3,
  };

  static sRemoteTabsPerPage = 25;
  static sRemoteTabsNextPageMinTabs = 5;

  constructor(panelview, deck, tabsList, separator) {
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);

    Services.obs.addObserver(this, SyncedTabs.TOPIC_TABS_CHANGED, true);
    this.deck = deck;
    this.tabsList = tabsList;
    this.separator = separator;
    this._showSyncedTabsPromise = Promise.resolve();

    this.createSyncedTabs();
  }

  observe(subject, topic, data) {
    if (topic == SyncedTabs.TOPIC_TABS_CHANGED) {
      this._showSyncedTabs();
    }
  }

  createSyncedTabs() {
    if (SyncedTabs.isConfiguredToSyncTabs) {
      if (SyncedTabs.hasSyncedThisSession) {
        this.deck.selectedIndex =
          SyncedTabsPanelList.sRemoteTabsDeckIndices.DECKINDEX_TABS;
      } else {
        // Sync hasn't synced tabs yet, so show the "fetching" panel.
        this.deck.selectedIndex =
          SyncedTabsPanelList.sRemoteTabsDeckIndices.DECKINDEX_FETCHING;
      }
      // force a background sync.
      SyncedTabs.syncTabs().catch(ex => {
        Cu.reportError(ex);
      });
      this.deck.toggleAttribute("syncingtabs", true);
      // show the current list - it will be updated by our observer.
      this._showSyncedTabs();
      if (this.separator) {
        this.separator.hidden = false;
      }
    } else {
      // not configured to sync tabs, so no point updating the list.
      this.deck.selectedIndex =
        SyncedTabsPanelList.sRemoteTabsDeckIndices.DECKINDEX_TABSDISABLED;
      this.deck.toggleAttribute("syncingtabs", false);
      if (this.separator) {
        this.separator.hidden = true;
      }
    }
  }

  // Update the synced tab list after any existing in-flight updates are complete.
  _showSyncedTabs(paginationInfo) {
    this._showSyncedTabsPromise = this._showSyncedTabsPromise.then(
      () => {
        return this.__showSyncedTabs(paginationInfo);
      },
      e => {
        Cu.reportError(e);
      }
    );
  }

  // Return a new promise to update the tab list.
  __showSyncedTabs(paginationInfo) {
    if (!this.tabsList) {
      // Closed between the previous `this._showSyncedTabsPromise`
      // resolving and now.
      return undefined;
    }
    const syncPrefsButtonEl = PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-sync-prefs-button"
    );
    return SyncedTabs.getTabClients()
      .then(clients => {
        let noTabs = !UIState.get().syncEnabled || !clients.length;
        this.deck.toggleAttribute("syncingtabs", !noTabs);
        if (this.separator) {
          this.separator.hidden = noTabs;
        }

        syncPrefsButtonEl.hidden =
          !UIState.get().syncEnabled || clients.length <= 1;

        // The view may have been hidden while the promise was resolving.
        if (!this.tabsList) {
          return;
        }
        if (clients.length === 0 && !SyncedTabs.hasSyncedThisSession) {
          // the "fetching tabs" deck is being shown - let's leave it there.
          // When that first sync completes we'll be notified and update.
          return;
        }

        if (clients.length === 0) {
          this.deck.selectedIndex =
            SyncedTabsPanelList.sRemoteTabsDeckIndices.DECKINDEX_NOCLIENTS;
          return;
        }
        this.deck.selectedIndex =
          SyncedTabsPanelList.sRemoteTabsDeckIndices.DECKINDEX_TABS;
        this._clearSyncedTabList();
        SyncedTabs.sortTabClientsByLastUsed(clients);
        let fragment = document.createDocumentFragment();

        let clientNumber = 0;
        for (let client of clients) {
          // add a menu separator for all clients other than the first.
          if (fragment.lastElementChild) {
            let separator = document.createXULElement("menuseparator");
            fragment.appendChild(separator);
          }
          // We add the client's elements to a container, and indicate which
          // element labels it.
          let labelId = `synced-tabs-client-${clientNumber++}`;
          let container = document.createXULElement("vbox");
          container.classList.add("PanelUI-remotetabs-clientcontainer");
          container.setAttribute("role", "group");
          container.setAttribute("aria-labelledby", labelId);
          if (paginationInfo && paginationInfo.clientId == client.id) {
            this._appendSyncClient(
              client,
              container,
              labelId,
              paginationInfo.maxTabs
            );
          } else {
            this._appendSyncClient(client, container, labelId);
          }
          fragment.appendChild(container);
        }
        this.tabsList.appendChild(fragment);
        PanelView.forNode(
          this.tabsList.closest("panelview")
        ).descriptionHeightWorkaround();
      })
      .catch(err => {
        Cu.reportError(err);
      })
      .then(() => {
        // an observer for tests.
        Services.obs.notifyObservers(
          null,
          "synced-tabs-menu:test:tabs-updated"
        );
      });
  }

  _clearSyncedTabList() {
    let list = this.tabsList;
    while (list.lastChild) {
      list.lastChild.remove();
    }
  }

  _appendMessageLabel(messageAttr, appendTo = null) {
    if (!appendTo) {
      appendTo = this.tabsList;
    }
    let message = this.tabsList.getAttribute(messageAttr);
    let messageLabel = document.createXULElement("label");
    messageLabel.textContent = message;
    appendTo.appendChild(messageLabel);
    return messageLabel;
  }

  _appendSyncClient(
    client,
    container,
    labelId,
    maxTabs = SyncedTabsPanelList.sRemoteTabsPerPage
  ) {
    // Create the element for the remote client.
    let clientItem = document.createXULElement("label");
    clientItem.setAttribute("id", labelId);
    clientItem.setAttribute("itemtype", "client");
    clientItem.setAttribute(
      "tooltiptext",
      gSync.fluentStrings.formatValueSync("appmenu-fxa-last-sync", {
        time: gSync.formatLastSyncDate(new Date(client.lastModified)),
      })
    );
    clientItem.textContent = client.name;

    container.appendChild(clientItem);

    if (!client.tabs.length) {
      let label = this._appendMessageLabel("notabsforclientlabel", container);
      label.setAttribute("class", "PanelUI-remotetabs-notabsforclient-label");
    } else {
      // If this page will display all tabs, show no additional buttons.
      // If the next page will display all the remaining tabs, show a "Show All" button
      // Otherwise, show a "Shore More" button
      let hasNextPage = client.tabs.length > maxTabs;
      let nextPageIsLastPage =
        hasNextPage &&
        maxTabs + SyncedTabsPanelList.sRemoteTabsPerPage >= client.tabs.length;
      if (nextPageIsLastPage) {
        // When the user clicks "Show All", try to have at least sRemoteTabsNextPageMinTabs more tabs
        // to display in order to avoid user frustration
        maxTabs = Math.min(
          client.tabs.length - SyncedTabsPanelList.sRemoteTabsNextPageMinTabs,
          maxTabs
        );
      }
      if (hasNextPage) {
        client.tabs = client.tabs.slice(0, maxTabs);
      }
      for (let tab of client.tabs) {
        let tabEnt = this._createSyncedTabElement(tab);
        container.appendChild(tabEnt);
      }
      if (hasNextPage) {
        let showAllEnt = this._createShowMoreSyncedTabsElement(client.id);
        container.appendChild(showAllEnt);
      }
    }
  }

  _createSyncedTabElement(tabInfo) {
    let item = document.createXULElement("toolbarbutton");
    let tooltipText = (tabInfo.title ? tabInfo.title + "\n" : "") + tabInfo.url;
    item.setAttribute("itemtype", "tab");
    item.setAttribute("class", "subviewbutton");
    item.setAttribute("targetURI", tabInfo.url);
    item.setAttribute(
      "label",
      tabInfo.title != "" ? tabInfo.title : tabInfo.url
    );
    item.setAttribute("image", tabInfo.icon);
    item.setAttribute("tooltiptext", tooltipText);
    // We need to use "click" instead of "command" here so openUILink
    // respects different buttons (eg, to open in a new tab).
    item.addEventListener("click", e => {
      document.defaultView.openUILink(tabInfo.url, e, {
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
      });
      if (document.defaultView.whereToOpenLink(e) != "current") {
        e.preventDefault();
        e.stopPropagation();
      } else {
        CustomizableUI.hidePanelForNode(item);
      }
    });
    return item;
  }

  _createShowMoreSyncedTabsElement(clientId) {
    let labelAttr = "showMoreLabel";
    let tooltipAttr = "showMoreTooltipText";
    let showCount = Infinity;

    let showMoreItem = document.createXULElement("toolbarbutton");
    showMoreItem.setAttribute("itemtype", "showmorebutton");
    showMoreItem.classList.add(
      "subviewbutton",
      "subviewbutton-nav",
      "subviewbutton-nav-down"
    );
    let label = gSync.fluentStrings.formatValueSync(
      this.tabsList.getAttribute(labelAttr)
    );
    showMoreItem.setAttribute("label", label);
    let tooltipText = this.tabsList.getAttribute(tooltipAttr);
    showMoreItem.setAttribute("tooltiptext", tooltipText);
    showMoreItem.addEventListener("click", e => {
      e.preventDefault();
      e.stopPropagation();
      this._showSyncedTabs({ clientId, maxTabs: showCount });
    });
    return showMoreItem;
  }

  destroy() {
    Services.obs.removeObserver(this, SyncedTabs.TOPIC_TABS_CHANGED);
    this.tabsList = null;
    this.deck = null;
    this.separator = null;
  }
};

var gSync = {
  _initialized: false,
  _isCurrentlySyncing: false,
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

  get fluentStrings() {
    delete this.fluentStrings;
    return (this.fluentStrings = new Localization(
      [
        "branding/brand.ftl",
        "browser/accounts.ftl",
        "browser/appmenu.ftl",
        "browser/sync.ftl",
        "browser/branding/sync-brand.ftl",
      ],
      true
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

  shouldHideSendContextMenuItems(enabled) {
    const state = UIState.get();
    // Only show the "Send..." context menu items when sending would be possible
    if (
      enabled &&
      state.status == UIState.STATUS_SIGNED_IN &&
      state.syncEnabled &&
      this.getSendTabTargets().length
    ) {
      return false;
    }
    return true;
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

  _definePrefGetters() {
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

    // Label for the sync buttons.
    const appMenuLabel = PanelMultiView.getViewNode(
      document,
      PanelUI.protonAppMenuEnabled ? "appMenu-fxa-label2" : "appMenu-fxa-label"
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

    if (PanelUI.protonAppMenuEnabled) {
      const appMenuHeaderTitle = PanelMultiView.getViewNode(
        document,
        "appMenu-header-title"
      );
      const appMenuHeaderDescription = PanelMultiView.getViewNode(
        document,
        "appMenu-header-description"
      );
      const appMenuHeaderText = PanelMultiView.getViewNode(
        document,
        "appMenu-fxa-text"
      );
      appMenuHeaderTitle.hidden = true;
      // We must initialize the label attribute here instead of the markup
      // due to a timing error. The fluent label attribute was being applied
      // after we had updated appMenuLabel and thus displayed an incorrect
      // label for signed in users.
      appMenuHeaderDescription.value = this.fluentStrings.formatValueSync(
        "appmenu-fxa-signed-in-label"
      );
      appMenuHeaderText.textContent = this.fluentStrings.formatValueSync(
        "appmenu-fxa-sync-and-save-data2"
      );
    }

    for (let topic of this._obs) {
      Services.obs.addObserver(this, topic, true);
    }

    this.maybeUpdateUIState();

    EnsureFxAccountsWebChannel();

    let fxaPanelView = PanelMultiView.getViewNode(document, "PanelUI-fxa");
    fxaPanelView.addEventListener("ViewShowing", this);
    fxaPanelView.addEventListener("ViewHiding", this);

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

  handleEvent(event) {
    switch (event.type) {
      case "ViewShowing": {
        this.onFxAPanelViewShowing(event.target);
        break;
      }
      case "ViewHiding": {
        this.onFxAPanelViewHiding(event.target);
      }
    }
  },

  onFxAPanelViewShowing(panelview) {
    let syncNowBtn = panelview.querySelector(".syncnow-label");
    let l10nId = syncNowBtn.getAttribute(
      this._isCurrentlySyncing
        ? "syncing-data-l10n-id"
        : "sync-now-data-l10n-id"
    );
    syncNowBtn.setAttribute("data-l10n-id", l10nId);

    panelview.syncedTabsPanelList = new SyncedTabsPanelList(
      panelview,
      PanelMultiView.getViewNode(document, "PanelUI-fxa-remotetabs-deck"),
      PanelMultiView.getViewNode(document, "PanelUI-fxa-remotetabs-tabslist"),
      PanelMultiView.getViewNode(document, "PanelUI-remote-tabs-separator")
    );
  },

  onFxAPanelViewHiding(panelview) {
    panelview.syncedTabsPanelList.destroy();
    panelview.syncedTabsPanelList = null;
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
            let lastSyncDate = gSync.formatLastSyncDate(lastModified);
            if (lastSyncDate) {
              item.setAttribute(
                "tooltiptext",
                this.fluentStrings.formatValueSync("appmenu-fxa-last-sync", {
                  time: lastSyncDate,
                })
              );
            }
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

    // We read the state that's been set on the root node, since that makes
    // it easier to test the various front-end states without having to actually
    // have UIState know about it.
    let fxaStatus = document.documentElement.getAttribute("fxastatus");

    if (PanelUI.protonAppMenuEnabled && fxaStatus == "not_configured") {
      this.openFxAEmailFirstPageFromFxaMenu(
        PanelMultiView.getViewNode(document, "PanelUI-fxa")
      );
      PanelUI.hide();
      return;
    }

    if (!gFxaToolbarAccessed) {
      Services.prefs.setBoolPref("identity.fxaccounts.toolbar.accessed", true);
    }
    const fxaSignOutButtonEl = PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-account-signout-button"
    );
    const fxaSignOutSeparator = PanelMultiView.getViewNode(
      document,
      "PanelUI-sign-out-separator"
    );
    fxaSignOutButtonEl.hidden = fxaSignOutSeparator.hidden =
      UIState.get() === UIState.STATUS_NOT_CONFIGURED;

    this.enableSendTabIfValidTab();

    if (!this.getSendTabTargets().length) {
      PanelMultiView.getViewNode(
        document,
        "PanelUI-fxa-menu-sendtab-button"
      ).hidden = true;
    }

    if (anchor.getAttribute("open") == "true") {
      PanelUI.hide();
    } else {
      this.emitFxaToolbarTelemetry("toolbar_icon", anchor);
      PanelUI.showSubView("PanelUI-fxa", anchor, aEvent);
    }
  },

  updateFxAPanel(state = {}) {
    const mainWindowEl = document.documentElement;

    // The Firefox Account toolbar currently handles 3 different states for
    // users. The default `not_configured` state shows an empty avatar, `unverified`
    // state shows an avatar with an email icon, `login-failed` state shows an avatar
    // with a danger icon and the `verified` state will show the users
    // custom profile image or a filled avatar.
    // In the proton app menu, the avatar is not shown.
    if (PanelUI.protonAppMenuEnabled) {
      PanelMultiView.getViewNode(document, "fxa-menu-avatar").hidden = true;
    }
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
    let headerDescription = this.fluentStrings.formatValueSync(
      "fxa-menu-turn-on-sync-default"
    );

    if (PanelUI.protonAppMenuEnabled) {
      let toolbarbuttons = fxaMenuPanel.querySelectorAll("toolbarbutton");
      for (let toolbarbutton of toolbarbuttons) {
        toolbarbutton.classList.remove("subviewbutton-iconic");
      }
    }

    cadButtonEl.setAttribute("disabled", true);
    syncNowButtonEl.hidden = true;
    fxaMenuAccountButtonEl.classList.remove("subviewbutton-nav");
    fxaMenuAccountButtonEl.removeAttribute("closemenu");
    syncSetupButtonEl.removeAttribute("hidden");

    if (state.status === UIState.STATUS_NOT_CONFIGURED) {
      mainWindowEl.style.removeProperty("--avatar-image-url");
      if (PanelUI.protonAppMenuEnabled) {
        headerDescription = this.fluentStrings.formatValueSync(
          "appmenu-fxa-signed-in-label"
        );
      }
    } else if (state.status === UIState.STATUS_LOGIN_FAILED) {
      stateValue = "login-failed";
      headerTitle = this.fluentStrings.formatValueSync("account-disconnected2");
      headerDescription = state.email;
      mainWindowEl.style.removeProperty("--avatar-image-url");
    } else if (state.status === UIState.STATUS_NOT_VERIFIED) {
      stateValue = "unverified";
      headerTitle = this.fluentStrings.formatValueSync(
        "account-finish-account-setup"
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
        syncSetupButtonEl.hidden = true;
      }

      headerTitle = this.fluentStrings.formatValueSync(
        "appmenuitem-fxa-manage-account"
      );

      headerDescription = state.email;
    }
    mainWindowEl.setAttribute("fxastatus", stateValue);

    menuHeaderTitleEl.value = headerTitle;
    menuHeaderDescriptionEl.value = headerDescription;
    // We remove the data-l10n-id attribute here to prevent the node's value
    // attribute from being overwritten by Fluent when the panel is moved
    // around in the DOM.
    menuHeaderDescriptionEl.removeAttribute("data-l10n-id");
  },

  enableSendTabIfValidTab() {
    // All tabs selected must be sendable for the Send Tab button to be enabled
    // on the FxA menu.
    let canSendAllURIs = gBrowser.selectedTabs.every(t =>
      BrowserUtils.isShareableURL(t.linkedBrowser.currentURI)
    );

    PanelMultiView.getViewNode(
      document,
      "PanelUI-fxa-menu-sendtab-button"
    ).hidden = !canSendAllURIs;
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
      PanelUI.protonAppMenuEnabled
        ? "appMenu-fxa-status2"
        : "appMenu-fxa-status"
    );
    const appMenuLabel = PanelMultiView.getViewNode(
      document,
      PanelUI.protonAppMenuEnabled ? "appMenu-fxa-label2" : "appMenu-fxa-label"
    );
    const appMenuAvatar = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-avatar"
    );
    const appMenuHeaderText = PanelMultiView.getViewNode(
      document,
      "appMenu-fxa-text"
    );
    const appMenuHeaderTitle = PanelMultiView.getViewNode(
      document,
      "appMenu-header-title"
    );
    const appMenuHeaderDescription = PanelMultiView.getViewNode(
      document,
      "appMenu-header-description"
    );
    const fxaPanelView = PanelMultiView.getViewNode(document, "PanelUI-fxa");

    let defaultLabel = PanelUI.protonAppMenuEnabled
      ? this.fluentStrings.formatValueSync("appmenu-fxa-signed-in-label")
      : appMenuStatus.getAttribute("defaultlabel");
    const status = state.status;
    // Reset the status bar to its original state.
    appMenuLabel.setAttribute("label", defaultLabel);
    appMenuLabel.removeAttribute("aria-labelledby");
    appMenuStatus.removeAttribute("fxastatus");
    appMenuAvatar.style.removeProperty("list-style-image");

    if (status == UIState.STATUS_NOT_CONFIGURED) {
      if (PanelUI.protonAppMenuEnabled) {
        appMenuHeaderText.hidden = false;
        appMenuHeaderText.style.visibility = "visible";
        appMenuStatus.classList.add("toolbaritem-combined-buttons");
        appMenuLabel.classList.remove("subviewbutton-nav");
        appMenuHeaderTitle.hidden = true;
        appMenuHeaderDescription.value = defaultLabel;
      }
      return;
    }
    appMenuLabel.classList.remove("subviewbutton-nav");

    if (PanelUI.protonAppMenuEnabled) {
      appMenuHeaderText.style.visibility = "collapse";
      appMenuStatus.classList.remove("toolbaritem-combined-buttons");
    }

    // At this point we consider sync to be configured (but still can be in an error state).
    if (status == UIState.STATUS_LOGIN_FAILED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName(
        "reconnectDescription",
        [state.email]
      );
      appMenuStatus.setAttribute("fxastatus", "login-failed");
      let errorLabel = this.fluentStrings.formatValueSync(
        "account-disconnected2"
      );
      appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      if (PanelUI.protonAppMenuEnabled) {
        appMenuLabel.classList.add("subviewbutton-nav");
        appMenuHeaderTitle.hidden = false;
        appMenuHeaderTitle.value = errorLabel;
        appMenuHeaderDescription.value = state.email;

        appMenuLabel.removeAttribute("label");
        appMenuLabel.setAttribute(
          "aria-labelledby",
          `${appMenuHeaderTitle.id},${appMenuHeaderDescription.id}`
        );
      } else {
        appMenuLabel.setAttribute("label", errorLabel);
      }
      return;
    } else if (status == UIState.STATUS_NOT_VERIFIED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName(
        "verifyDescription",
        [state.email]
      );
      appMenuStatus.setAttribute("fxastatus", "unverified");
      let unverifiedLabel = this.fluentStrings.formatValueSync(
        "account-finish-account-setup"
      );
      appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      if (PanelUI.protonAppMenuEnabled) {
        appMenuLabel.classList.add("subviewbutton-nav");
        appMenuHeaderTitle.hidden = false;
        appMenuHeaderTitle.value = unverifiedLabel;
        appMenuHeaderDescription.value = state.email;

        appMenuLabel.removeAttribute("label");
        appMenuLabel.setAttribute(
          "aria-labelledby",
          `${appMenuHeaderTitle.id},${appMenuHeaderDescription.id}`
        );
      } else {
        appMenuLabel.setAttribute("label", unverifiedLabel);
      }
      return;
    }

    // At this point we consider sync to be logged-in.
    if (PanelUI.protonAppMenuEnabled) {
      appMenuHeaderTitle.hidden = true;
      appMenuHeaderDescription.value = state.email;
    } else {
      appMenuLabel.classList.add("subviewbutton-iconic");
    }
    appMenuStatus.setAttribute("fxastatus", "signedin");
    appMenuLabel.setAttribute("label", state.email);
    appMenuLabel.classList.add("subviewbutton-nav");
    fxaPanelView.setAttribute(
      "title",
      this.fluentStrings.formatValueSync("appmenu-fxa-header2")
    );
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
        this.openFxAManagePageFromFxaMenu(panel);
    }
  },

  async openFxAEmailFirstPage(entryPoint) {
    const url = await FxAccounts.config.promiseConnectAccountURI(entryPoint);
    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  async openFxAEmailFirstPageFromFxaMenu(panel = undefined) {
    this.emitFxaToolbarTelemetry("login", panel);
    let entryPoint = "fxa_discoverability_native";
    if (
      this.isPanelInsideAppMenu(panel) ||
      (PanelUI.protonAppMenuEnabled && panel)
    ) {
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
        // Show the Sent! confirmation if any of the sends succeeded.
        if (results.includes(true)) {
          // FxA button could be hidden with CSS since the user is logged out,
          // although it seems likely this would only happen in testing...
          let fxastatus = document.documentElement.getAttribute("fxastatus");
          let anchorNode =
            (fxastatus &&
              fxastatus != "not_configured" &&
              document.getElementById("fxa-toolbar-menu-button")) ||
            document.getElementById("PanelUI-menu-button");
          ConfirmationHint.show(anchorNode, "sendToDevice");
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

    if (targets.length > 1) {
      // "Send to All Devices" menu item
      const separator = createDeviceNodeFn();
      separator.classList.add("sync-menuitem");
      fragment.appendChild(separator);
      const allDevicesLabel = this.fxaStrings.GetStringFromName(
        "sendToAllDevices.menuitem"
      );
      addTargetDevice("", allDevicesLabel, "");

      // "Manage devices" menu item
      const manageDevicesLabel = this.fxaStrings.GetStringFromName(
        "manageDevices.menuitem"
      );
      // We piggyback on the createDeviceNodeFn implementation,
      // it's a big disgusting.
      const targetDevice = createDeviceNodeFn(
        null,
        manageDevicesLabel,
        null,
        null
      );
      targetDevice.addEventListener(
        "command",
        () => gSync.openDevicesManagementPage("sendtab"),
        true
      );
      targetDevice.classList.add("sync-menuitem", "sendtab-target");
      targetDevice.setAttribute("label", manageDevicesLabel);
      fragment.appendChild(targetDevice);
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
      if (BrowserUtils.isShareableURL(tab.linkedBrowser.currentURI)) {
        hasASendableURI = true;
        break;
      }
    }
    const enabled = !this.sendTabConfiguredAndLoading && hasASendableURI;
    const hideItems = this.shouldHideSendContextMenuItems(enabled);

    let sendTabsToDevice = document.getElementById("context_sendTabToDevice");
    sendTabsToDevice.disabled = !enabled;

    if (hideItems || !hasASendableURI) {
      sendTabsToDevice.hidden = true;
    } else {
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
      sendTabsToDevice.hidden = false;
    }
  },

  // "Send Page to Device" and "Send Link to Device" menu items
  updateContentContextMenu(contextMenu) {
    if (!this.FXA_ENABLED) {
      // These items are hidden by default. No need to do anything.
      return false;
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

    const targetURI = showSendLink
      ? contextMenu.getLinkURI()
      : contextMenu.browser.currentURI;
    const enabled =
      !this.sendTabConfiguredAndLoading &&
      BrowserUtils.isShareableURL(targetURI);
    const hideItems = this.shouldHideSendContextMenuItems(enabled);

    contextMenu.showItem(
      "context-sendpagetodevice",
      !hideItems && showSendPage
    );
    contextMenu.showItem(
      "context-sendlinktodevice",
      !hideItems && showSendLink
    );

    if (!showSendLink && !showSendPage) {
      return false;
    }

    contextMenu.setItemAttr(
      showSendPage ? "context-sendpagetodevice" : "context-sendlinktodevice",
      "disabled",
      !enabled || null
    );
    // return true if context menu items are visible
    return !hideItems && (showSendPage || showSendLink);
  },

  // Functions called by observers
  onActivityStart() {
    this._isCurrentlySyncing = true;
    clearTimeout(this._syncAnimationTimer);
    this._syncStartTime = Date.now();

    document.querySelectorAll(".syncnow-label").forEach(el => {
      let l10nId = el.getAttribute("syncing-data-l10n-id");
      el.setAttribute("data-l10n-id", l10nId);
    });

    document.querySelectorAll(".syncNowBtn").forEach(el => {
      el.setAttribute("syncstatus", "active");
    });

    document
      .getElementById("appMenu-viewCache")
      .content.querySelectorAll(".syncNowBtn")
      .forEach(el => {
        el.setAttribute("syncstatus", "active");
      });
  },

  _onActivityStop() {
    this._isCurrentlySyncing = false;
    if (!gBrowser) {
      return;
    }

    document.querySelectorAll(".syncnow-label").forEach(el => {
      let l10nId = el.getAttribute("sync-now-data-l10n-id");
      el.setAttribute("data-l10n-id", l10nId);
    });

    document.querySelectorAll(".syncNowBtn").forEach(el => {
      el.removeAttribute("syncstatus");
    });

    document
      .getElementById("appMenu-viewCache")
      .content.querySelectorAll(".syncNowBtn")
      .forEach(el => {
        el.removeAttribute("syncstatus");
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
      deleteLocalData: false,
    };

    let [title, body, button, checkbox] = await document.l10n.formatValues([
      { id: "fxa-signout-dialog2-title" },
      { id: "fxa-signout-dialog-body" },
      { id: "fxa-signout-dialog2-button" },
      { id: "fxa-signout-dialog2-checkbox" },
    ]);

    const flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1;

    if (!UIState.get().syncEnabled) {
      checkbox = null;
    }

    const result = await Services.prompt.asyncConfirmEx(
      window.browsingContext,
      Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
      title,
      body,
      flags,
      button,
      null,
      null,
      checkbox,
      false
    );
    const propBag = result.QueryInterface(Ci.nsIPropertyBag2);
    options.userConfirmedDisconnect = propBag.get("buttonNumClicked") == 0;
    options.deleteLocalData = propBag.get("checked");

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
    const [title, body, button] = await document.l10n.formatValues([
      { id: `sync-disconnect-dialog-title2` },
      { id: `sync-disconnect-dialog-body` },
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
      let lastSyncDate = this.formatLastSyncDate(state.lastSync);
      if (lastSyncDate) {
        tooltiptext = this.fluentStrings.formatValueSync(
          "appmenu-fxa-last-sync",
          { time: lastSyncDate }
        );
      }
    }

    let syncNowBtns = [
      "PanelUI-remotetabs-syncnow",
      "PanelUI-fxa-menu-syncnow-button",
    ];
    syncNowBtns.forEach(id => {
      let el = PanelMultiView.getViewNode(document, id);
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
      let adjustedDate = new Date(Date.now() - 1000);
      let relativeDateStr = this.relativeTimeFormat.formatBestUnit(
        date < adjustedDate ? date : adjustedDate
      );
      return relativeDateStr;
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
    document.documentElement.setAttribute("fxadisabled", true);

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
