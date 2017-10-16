/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "AppMenuNotifications",
                                  "resource://gre/modules/AppMenuNotifications.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ScrollbarSampler",
                                  "resource:///modules/ScrollbarSampler.jsm");

/**
 * Maintains the state and dispatches events for the main menu panel.
 */

const PanelUI = {
  /** Panel events that we listen for. **/
  get kEvents() {
    return ["popupshowing", "popupshown", "popuphiding", "popuphidden"];
  },
  /**
   * Used for lazily getting and memoizing elements from the document. Lazy
   * getters are set in init, and memoizing happens after the first retrieval.
   */
  get kElements() {
    return {
      mainView: "appMenu-mainView",
      multiView: "appMenu-multiView",
      helpView: "PanelUI-helpView",
      libraryView: "appMenu-libraryView",
      menuButton: "PanelUI-menu-button",
      panel: "appMenu-popup",
      notificationPanel: "appMenu-notification-popup",
      addonNotificationContainer: "appMenu-addon-banners",
      overflowFixedList: "widget-overflow-fixed-list",
      overflowPanel: "widget-overflow",
      navbar: "nav-bar",
    };
  },

  _initialized: false,
  _notifications: null,

  init() {
    this._initElements();

    this.menuButton.addEventListener("mousedown", this);
    this.menuButton.addEventListener("keypress", this);
    this._overlayScrollListenerBoundFn = this._overlayScrollListener.bind(this);

    Services.obs.addObserver(this, "fullscreen-nav-toolbox");
    Services.obs.addObserver(this, "appMenu-notifications");

    XPCOMUtils.defineLazyPreferenceGetter(this, "autoHideToolbarInFullScreen",
      "browser.fullscreen.autohide", false, (pref, previousValue, newValue) => {
        // On OSX, or with autohide preffed off, MozDOMFullscreen is the only
        // event we care about, since fullscreen should behave just like non
        // fullscreen. Otherwise, we don't want to listen to these because
        // we'd just be spamming ourselves with both of them whenever a user
        // opened a video.
        if (newValue) {
          window.removeEventListener("MozDOMFullscreen:Entered", this);
          window.removeEventListener("MozDOMFullscreen:Exited", this);
          window.addEventListener("fullscreen", this);
        } else {
          window.addEventListener("MozDOMFullscreen:Entered", this);
          window.addEventListener("MozDOMFullscreen:Exited", this);
          window.removeEventListener("fullscreen", this);
        }

        this._updateNotifications(false);
      }, autoHidePref => autoHidePref && Services.appinfo.OS !== "Darwin");

    if (this.autoHideToolbarInFullScreen) {
      window.addEventListener("fullscreen", this);
    } else {
      window.addEventListener("MozDOMFullscreen:Entered", this);
      window.addEventListener("MozDOMFullscreen:Exited", this);
    }

    XPCOMUtils.defineLazyPreferenceGetter(this, "libraryRecentHighlightsEnabled",
      "browser.library.activity-stream.enabled", false, (pref, previousValue, newValue) => {
        if (!newValue)
          this.clearLibraryRecentHighlights();
      });

    window.addEventListener("activate", this);
    window.matchMedia("(-moz-overlay-scrollbars)").addListener(this._overlayScrollListenerBoundFn);
    CustomizableUI.addListener(this);

    for (let event of this.kEvents) {
      this.notificationPanel.addEventListener(event, this);
    }

    // We do this sync on init because in order to have the overflow button show up
    // we need to know whether anything is in the permanent panel area.
    this.overflowFixedList.hidden = false;
    // Also unhide the separator. We use CSS to hide/show it based on the panel's content.
    this.overflowFixedList.previousSibling.hidden = false;
    CustomizableUI.registerMenuPanel(this.overflowFixedList, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
    this.updateOverflowStatus();

    Services.obs.notifyObservers(null, "appMenu-notifications-request", "refresh");

    this._initialized = true;
  },

  _initElements() {
    for (let [k, v] of Object.entries(this.kElements)) {
      // Need to do fresh let-bindings per iteration
      let getKey = k;
      let id = v;
      this.__defineGetter__(getKey, function() {
        delete this[getKey];
        return this[getKey] = document.getElementById(id);
      });
    }
  },

  _eventListenersAdded: false,
  _ensureEventListenersAdded() {
    if (this._eventListenersAdded)
      return;
    this._addEventListeners();
  },

  _addEventListeners() {
    for (let event of this.kEvents) {
      this.panel.addEventListener(event, this);
    }

    this.helpView.addEventListener("ViewShowing", this._onHelpViewShow);
    this._eventListenersAdded = true;
  },

  _removeEventListeners() {
    for (let event of this.kEvents) {
      this.panel.removeEventListener(event, this);
    }
    this.helpView.removeEventListener("ViewShowing", this._onHelpViewShow);
    this._eventListenersAdded = false;
  },

  uninit() {
    this._removeEventListeners();
    for (let event of this.kEvents) {
      this.notificationPanel.removeEventListener(event, this);
    }

    Services.obs.removeObserver(this, "fullscreen-nav-toolbox");
    Services.obs.removeObserver(this, "appMenu-notifications");

    window.removeEventListener("MozDOMFullscreen:Entered", this);
    window.removeEventListener("MozDOMFullscreen:Exited", this);
    window.removeEventListener("fullscreen", this);
    window.removeEventListener("activate", this);
    this.menuButton.removeEventListener("mousedown", this);
    this.menuButton.removeEventListener("keypress", this);
    window.matchMedia("(-moz-overlay-scrollbars)").removeListener(this._overlayScrollListenerBoundFn);
    CustomizableUI.removeListener(this);
    this._overlayScrollListenerBoundFn = null;
    this.libraryView.removeEventListener("ViewShowing", this);
  },

  /**
   * Customize mode extracts the mainView and puts it somewhere else while the
   * user customizes. Upon completion, this function can be called to put the
   * panel back to where it belongs in normal browsing mode.
   *
   * @param aMainView
   *        The mainView node to put back into place.
   */
  setMainView(aMainView) {
    this._ensureEventListenersAdded();
    this.multiView.setMainView(aMainView);
  },

  /**
   * Opens the menu panel if it's closed, or closes it if it's
   * open.
   *
   * @param aEvent the event that triggers the toggle.
   */
  toggle(aEvent) {
    // Don't show the panel if the window is in customization mode,
    // since this button doubles as an exit path for the user in this case.
    if (document.documentElement.hasAttribute("customizing")) {
      return;
    }
    this._ensureEventListenersAdded();
    if (this.panel.state == "open") {
      this.hide();
    } else if (this.panel.state == "closed") {
      this.show(aEvent);
    }
  },

  /**
   * Opens the menu panel. If the event target has a child with the
   * toolbarbutton-icon attribute, the panel will be anchored on that child.
   * Otherwise, the panel is anchored on the event target itself.
   *
   * @param aEvent the event (if any) that triggers showing the menu.
   */
  show(aEvent) {
    this._ensureShortcutsShown();
    return new Promise(resolve => {
      this.ensureReady().then(() => {
        if (this.panel.state == "open" ||
            document.documentElement.hasAttribute("customizing")) {
          resolve();
          return;
        }

        let anchor;
        let domEvent = null;
        if (!aEvent ||
            aEvent.type == "command") {
          anchor = this.menuButton;
        } else {
          domEvent = aEvent;
          anchor = aEvent.target;
        }

        this.panel.addEventListener("popupshown", function() {
          resolve();
        }, {once: true});

        anchor = this._getPanelAnchor(anchor);
        this.panel.openPopup(anchor, { triggerEvent: domEvent });
      }, (reason) => {
        console.error("Error showing the PanelUI menu", reason);
      });
    });
  },

  /**
   * If the menu panel is being shown, hide it.
   */
  hide() {
    if (document.documentElement.hasAttribute("customizing")) {
      return;
    }

    this.panel.hidePopup();
  },

  observe(subject, topic, status) {
    switch (topic) {
      case "fullscreen-nav-toolbox":
        if (this._notifications) {
          this._updateNotifications(false);
        }
        break;
      case "appMenu-notifications":
        // Don't initialize twice.
        if (status == "init" && this._notifications) {
          break;
        }
        this._notifications = AppMenuNotifications.notifications;
        this._updateNotifications(true);
        break;
    }
  },

  handleEvent(aEvent) {
    // Ignore context menus and menu button menus showing and hiding:
    if (aEvent.type.startsWith("popup") &&
        aEvent.target != this.panel) {
      return;
    }
    switch (aEvent.type) {
      case "popupshowing":
        updateEditUIVisibility();
        // Fall through
      case "popupshown":
        if (aEvent.type == "popupshown") {
          CustomizableUI.addPanelCloseListeners(this.panel);
        }
        // Fall through
      case "popuphiding":
        if (aEvent.type == "popuphiding") {
          updateEditUIVisibility();
        }
        // Fall through
      case "popuphidden":
        this._updateNotifications();
        this._updatePanelButton(aEvent.target);
        if (aEvent.type == "popuphidden") {
          CustomizableUI.removePanelCloseListeners(this.panel);
        }
        break;
      case "mousedown":
        if (aEvent.button == 0)
          this.toggle(aEvent);
        break;
      case "keypress":
        this.toggle(aEvent);
        break;
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
      case "fullscreen":
      case "activate":
        this._updateNotifications();
        break;
      case "ViewShowing":
        if (aEvent.target == this.libraryView) {
          this.onLibraryViewShowing(aEvent.target);
        }
        break;
    }
  },

  get isReady() {
    return !!this._isReady;
  },

  get isNotificationPanelOpen() {
    let panelState = this.notificationPanel.state;

    return panelState == "showing" || panelState == "open";
  },

  /**
   * Registering the menu panel is done lazily for performance reasons. This
   * method is exposed so that CustomizationMode can force panel-readyness in the
   * event that customization mode is started before the panel has been opened
   * by the user.
   *
   * @param aCustomizing (optional) set to true if this was called while entering
   *        customization mode. If that's the case, we trust that customization
   *        mode will handle calling beginBatchUpdate and endBatchUpdate.
   *
   * @return a Promise that resolves once the panel is ready to roll.
   */
  async ensureReady() {
    if (this._isReady) {
      return;
    }

    await window.delayedStartupPromise;
    this._ensureEventListenersAdded();
    this.panel.hidden = false;
    this._isReady = true;
  },

  /**
   * Switch the panel to the main view if it's not already
   * in that view.
   */
  showMainView() {
    this._ensureEventListenersAdded();
    this.multiView.showMainView();
  },

  /**
   * Switch the panel to the help view if it's not already
   * in that view.
   */
  showHelpView(aAnchor) {
    this._ensureEventListenersAdded();
    this.multiView.showSubView("PanelUI-helpView", aAnchor);
  },

  /**
   * Shows a subview in the panel with a given ID.
   *
   * @param aViewId the ID of the subview to show.
   * @param aAnchor the element that spawned the subview.
   * @param aPlacementArea the CustomizableUI area that aAnchor is in.
   * @param aEvent the event triggering the view showing.
   */
  async showSubView(aViewId, aAnchor, aPlacementArea, aEvent) {

    let domEvent = null;
    if (aEvent) {
      if (aEvent.type == "mousedown" && aEvent.button != 0) {
        return;
      }
      if (aEvent.type == "command" && aEvent.inputSource != null) {
        // Synthesize a new DOM mouse event to pass on the inputSource.
        domEvent = document.createEvent("MouseEvent");
        domEvent.initNSMouseEvent("click", true, true, null, 0, aEvent.screenX, aEvent.screenY,
                                  0, 0, false, false, false, false, 0, aEvent.target, 0, aEvent.inputSource);
      } else if (aEvent.mozInputSource != null) {
        domEvent = aEvent;
      }
    }

    this._ensureEventListenersAdded();
    let viewNode = document.getElementById(aViewId);
    if (!viewNode) {
      Cu.reportError("Could not show panel subview with id: " + aViewId);
      return;
    }

    if (!aAnchor) {
      Cu.reportError("Expected an anchor when opening subview with id: " + aViewId);
      return;
    }

    this.ensureLibraryInitialized(viewNode);

    let container = aAnchor.closest("panelmultiview,photonpanelmultiview");
    if (container) {
      container.showSubView(aViewId, aAnchor);
    } else if (!aAnchor.open) {
      aAnchor.open = true;

      let tempPanel = document.createElement("panel");
      tempPanel.setAttribute("type", "arrow");
      tempPanel.setAttribute("id", "customizationui-widget-panel");
      tempPanel.setAttribute("class", "cui-widget-panel");
      tempPanel.setAttribute("viewId", aViewId);
      if (aAnchor.getAttribute("tabspecific")) {
        tempPanel.setAttribute("tabspecific", true);
      }
      if (this._disableAnimations) {
        tempPanel.setAttribute("animate", "false");
      }
      tempPanel.setAttribute("context", "");
      tempPanel.setAttribute("photon", true);
      document.getElementById(CustomizableUI.AREA_NAVBAR).appendChild(tempPanel);
      // If the view has a footer, set a convenience class on the panel.
      tempPanel.classList.toggle("cui-widget-panelWithFooter",
                                 viewNode.querySelector(".panel-subview-footer"));

      // If the panelview is already selected in another PanelMultiView instance
      // as a subview, make sure to properly hide it there.
      let oldMultiView = viewNode.panelMultiView;
      if (oldMultiView && oldMultiView.current == viewNode) {
        await oldMultiView.showMainView();
      }

      let viewShown = false;
      let listener = () => viewShown = true;
      viewNode.addEventListener("ViewShown", listener, {once: true});

      let multiView = document.createElement("photonpanelmultiview");
      multiView.setAttribute("id", "customizationui-widget-multiview");
      multiView.setAttribute("viewCacheId", "appMenu-viewCache");
      multiView.setAttribute("mainViewId", viewNode.id);
      multiView.setAttribute("ephemeral", true);
      document.getElementById("appMenu-viewCache").appendChild(viewNode);
      tempPanel.appendChild(multiView);
      viewNode.classList.add("cui-widget-panelview");

      let panelRemover = () => {
        viewNode.classList.remove("cui-widget-panelview");
        if (viewShown) {
          CustomizableUI.removePanelCloseListeners(tempPanel);
          tempPanel.removeEventListener("popuphidden", panelRemover);
        }
        aAnchor.open = false;

        // Ensure we run the destructor:
        multiView.instance.destructor();

        tempPanel.remove();
      };

      // Wait until all the tasks needed to show a view are done.
      await multiView.currentShowPromise;

      if (!viewShown) {
        viewNode.removeEventListener("ViewShown", listener);
        panelRemover();
        return;
      }

      CustomizableUI.addPanelCloseListeners(tempPanel);
      tempPanel.addEventListener("popuphidden", panelRemover);

      let anchor = this._getPanelAnchor(aAnchor);

      if (aAnchor != anchor && aAnchor.id) {
        anchor.setAttribute("consumeanchor", aAnchor.id);
      }

      tempPanel.openPopup(anchor, {
        position: "bottomcenter topright",
        triggerEvent: domEvent,
      });
    }
  },

  /**
   * Sets up the event listener for when the Library view is shown.
   *
   * @param {panelview} viewNode The library view.
   */
  ensureLibraryInitialized(viewNode) {
    if (viewNode != this.libraryView || viewNode._initialized)
      return;

    viewNode._initialized = true;
    viewNode.addEventListener("ViewShowing", this);
  },

  /**
   * When the Library view is showing, we can start fetching and populating the
   * list of Recent Highlights.
   * This is done asynchronously and may finish when the view is already visible.
   *
   * @param {panelview} viewNode The library view.
   */
  async onLibraryViewShowing(viewNode) {
    if (this._loadingRecentHighlights) {
      return;
    }
    this._loadingRecentHighlights = true;

    // Since the library is the first view shown, we don't want to add a blocker
    // to the event, which would make PanelMultiView wait to show it.
    let container = this.clearLibraryRecentHighlights();
    if (!this.libraryRecentHighlightsEnabled) {
      this._loadingRecentHighlights = false;
      return;
    }

    let highlights = await NewTabUtils.activityStreamLinks.getHighlights({
      // As per bug 1402023, hard-coded limit, until Activity Stream develops a
      // richer list.
      numItems: 6,
      withFavicons: true
    });
    // If there's nothing to display, or the panel is already hidden, get out.
    let multiView = viewNode.panelMultiView;
    if (!highlights.length || (multiView && multiView.getAttribute("panelopen") != "true")) {
      this._loadingRecentHighlights = false;
      return;
    }

    container.hidden = container.previousSibling.hidden =
      container.previousSibling.previousSibling.hidden = false;
    let fragment = document.createDocumentFragment();
    for (let highlight of highlights) {
      let button = document.createElement("toolbarbutton");
      button.classList.add("subviewbutton", "highlight", "subviewbutton-iconic", "bookmark-item");
      let title = highlight.title || highlight.url;
      button.setAttribute("label", title);
      button.setAttribute("tooltiptext", title);
      button.setAttribute("type", "highlight-" + highlight.type);
      button.setAttribute("onclick", "PanelUI.onLibraryHighlightClick(event)");
      if (highlight.favicon) {
        button.setAttribute("image", highlight.favicon);
      }
      button._highlight = highlight;
      fragment.appendChild(button);
    }
    container.appendChild(fragment);

    this._loadingRecentHighlights = false;
  },

  /**
   * Remove all the nodes from the 'Recent Highlights' section and hide it as well.
   */
  clearLibraryRecentHighlights() {
    let container = document.getElementById("appMenu-library-recentHighlights");
    while (container.firstChild) {
      container.firstChild.remove();
    }
    container.hidden = container.previousSibling.hidden =
      container.previousSibling.previousSibling.hidden = true;
    return container;
  },

  /**
   * Event handler; invoked when an item of the Recent Highlights is clicked.
   *
   * @param {MouseEvent} event Click event, originating from the Highlight.
   */
  onLibraryHighlightClick(event) {
    let button = event.target;
    if (event.button > 1 || !button._highlight) {
      return;
    }
    window.openUILink(button._highlight.url, event);
  },

  /**
   * NB: The enable- and disableSingleSubviewPanelAnimations methods only
   * affect the hiding/showing animations of single-subview panels (tempPanel
   * in the showSubView method).
   */
  disableSingleSubviewPanelAnimations() {
    this._disableAnimations = true;
  },

  enableSingleSubviewPanelAnimations() {
    this._disableAnimations = false;
  },

  updateOverflowStatus() {
    let hasKids = this.overflowFixedList.hasChildNodes();
    if (hasKids && !this.navbar.hasAttribute("nonemptyoverflow")) {
      this.navbar.setAttribute("nonemptyoverflow", "true");
      this.overflowPanel.setAttribute("hasfixeditems", "true");
    } else if (!hasKids && this.navbar.hasAttribute("nonemptyoverflow")) {
      if (this.overflowPanel.state != "closed") {
        this.overflowPanel.hidePopup();
      }
      this.overflowPanel.removeAttribute("hasfixeditems");
      this.navbar.removeAttribute("nonemptyoverflow");
    }
  },

  onWidgetAfterDOMChange(aNode, aNextNode, aContainer, aWasRemoval) {
    if (aContainer == this.overflowFixedList) {
      this.updateOverflowStatus();
    }
  },

  onAreaReset(aArea, aContainer) {
    if (aContainer == this.overflowFixedList) {
      this.updateOverflowStatus();
    }
  },

  /**
   * Sets the anchor node into the open or closed state, depending
   * on the state of the panel.
   */
  _updatePanelButton() {
    this.menuButton.open = this.panel.state == "open" ||
                           this.panel.state == "showing";
  },

  _onHelpViewShow(aEvent) {
    // Call global menu setup function
    buildHelpMenu();

    let helpMenu = document.getElementById("menu_HelpPopup");
    let items = this.getElementsByTagName("vbox")[0];
    let attrs = ["oncommand", "onclick", "label", "key", "disabled"];
    let NSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    // Remove all buttons from the view
    while (items.firstChild) {
      items.firstChild.remove();
    }

    // Add the current set of menuitems of the Help menu to this view
    let menuItems = Array.prototype.slice.call(helpMenu.getElementsByTagName("menuitem"));
    let fragment = document.createDocumentFragment();
    for (let node of menuItems) {
      if (node.hidden)
        continue;
      let button = document.createElementNS(NSXUL, "toolbarbutton");
      // Copy specific attributes from a menuitem of the Help menu
      for (let attrName of attrs) {
        if (!node.hasAttribute(attrName))
          continue;
        button.setAttribute(attrName, node.getAttribute(attrName));
      }
      button.setAttribute("class", "subviewbutton");
      fragment.appendChild(button);
    }
    items.appendChild(fragment);
  },

  _updateQuitTooltip() {
    if (AppConstants.platform == "win") {
      return;
    }

    let tooltipId = AppConstants.platform == "macosx" ?
                    "quit-button.tooltiptext.mac" :
                    "quit-button.tooltiptext.linux2";

    let brands = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    let stringArgs = [brands.GetStringFromName("brandShortName")];

    let key = document.getElementById("key_quitApplication");
    stringArgs.push(ShortcutUtils.prettifyShortcut(key));
    let tooltipString = CustomizableUI.getLocalizedProperty({x: tooltipId}, "x", stringArgs);
    let quitButton = document.getElementById("PanelUI-quit");
    quitButton.setAttribute("tooltiptext", tooltipString);
  },

  _overlayScrollListenerBoundFn: null,
  _overlayScrollListener(aMQL) {
    ScrollbarSampler.resetSystemScrollbarWidth();
    this._scrollWidth = null;
  },

  _hidePopup() {
    if (this.isNotificationPanelOpen) {
      this.notificationPanel.hidePopup();
    }
  },

  _updateNotifications(notificationsChanged) {
    let notifications = this._notifications;
    if (!notifications || !notifications.length) {
      if (notificationsChanged) {
        this._clearAllNotifications();
        this._hidePopup();
      }
      return;
    }

    if ((window.fullScreen && FullScreen.navToolboxHidden) || document.fullscreenElement) {
      this._hidePopup();
      return;
    }

    let doorhangers =
      notifications.filter(n => !n.dismissed && !n.options.badgeOnly);

    if (this.panel.state == "showing" || this.panel.state == "open") {
      // If the menu is already showing, then we need to dismiss all notifications
      // since we don't want their doorhangers competing for attention
      doorhangers.forEach(n => { n.dismissed = true; });
      this._hidePopup();
      this._clearBadge();
      if (!notifications[0].options.badgeOnly) {
        this._showBannerItem(notifications[0]);
      }
    } else if (doorhangers.length > 0) {
      // Only show the doorhanger if the window is focused and not fullscreen
      if ((window.fullScreen && this.autoHideToolbarInFullScreen) || Services.focus.activeWindow !== window) {
        this._hidePopup();
        this._showBadge(doorhangers[0]);
        this._showBannerItem(doorhangers[0]);
      } else {
        this._clearBadge();
        this._showNotificationPanel(doorhangers[0]);
      }
    } else {
      this._hidePopup();
      this._showBadge(notifications[0]);
      this._showBannerItem(notifications[0]);
    }
  },

  _showNotificationPanel(notification) {
    this._refreshNotificationPanel(notification);

    if (this.isNotificationPanelOpen) {
      return;
    }

    if (notification.options.beforeShowDoorhanger) {
      notification.options.beforeShowDoorhanger(document);
    }

    let anchor = this._getPanelAnchor(this.menuButton);

    this.notificationPanel.hidden = false;
    this.notificationPanel.openPopup(anchor, "bottomcenter topright");
  },

  _clearNotificationPanel() {
    for (let popupnotification of this.notificationPanel.children) {
      popupnotification.hidden = true;
      popupnotification.notification = null;
    }
  },

  _clearAllNotifications() {
    this._clearNotificationPanel();
    this._clearBadge();
    this._clearBannerItem();
  },

  _refreshNotificationPanel(notification) {
    this._clearNotificationPanel();

    let popupnotificationID = this._getPopupId(notification);
    let popupnotification = document.getElementById(popupnotificationID);

    popupnotification.setAttribute("id", popupnotificationID);
    popupnotification.setAttribute("buttoncommand", "PanelUI._onNotificationButtonEvent(event, 'buttoncommand');");
    popupnotification.setAttribute("secondarybuttoncommand",
      "PanelUI._onNotificationButtonEvent(event, 'secondarybuttoncommand');");

    popupnotification.notification = notification;
    popupnotification.hidden = false;
  },

  _showBadge(notification) {
    let badgeStatus = this._getBadgeStatus(notification);
    this.menuButton.setAttribute("badge-status", badgeStatus);
  },

  // "Banner item" here refers to an item in the hamburger panel menu. They will
  // typically show up as a colored row in the panel.
  _showBannerItem(notification) {
    if (!this._panelBannerItem) {
      this._panelBannerItem = this.mainView.querySelector(".panel-banner-item");
    }
    let label = this._panelBannerItem.getAttribute("label-" + notification.id);
    // Ignore items we don't know about.
    if (!label) {
      return;
    }
    this._panelBannerItem.setAttribute("notificationid", notification.id);
    this._panelBannerItem.setAttribute("label", label);
    this._panelBannerItem.hidden = false;
    this._panelBannerItem.notification = notification;
  },

  _clearBadge() {
    this.menuButton.removeAttribute("badge-status");
  },

  _clearBannerItem() {
    if (this._panelBannerItem) {
      this._panelBannerItem.notification = null;
      this._panelBannerItem.hidden = true;
    }
  },

  _onNotificationButtonEvent(event, type) {
    let notificationEl = getNotificationFromElement(event.originalTarget);

    if (!notificationEl)
      throw "PanelUI._onNotificationButtonEvent: couldn't find notification element";

    if (!notificationEl.notification)
      throw "PanelUI._onNotificationButtonEvent: couldn't find notification";

    let notification = notificationEl.notification;

    if (type == "secondarybuttoncommand") {
      AppMenuNotifications.callSecondaryAction(window, notification);
    } else {
      AppMenuNotifications.callMainAction(window, notification, true);
    }
  },

  _onBannerItemSelected(event) {
    let target = event.originalTarget;
    if (!target.notification)
      throw "menucommand target has no associated action/notification";

    event.stopPropagation();
    AppMenuNotifications.callMainAction(window, target.notification, false);
  },

  _getPopupId(notification) { return "appMenu-" + notification.id + "-notification"; },

  _getBadgeStatus(notification) { return notification.id; },

  _getPanelAnchor(candidate) {
    let iconAnchor =
      document.getAnonymousElementByAttribute(candidate, "class",
                                              "toolbarbutton-icon");
    return iconAnchor || candidate;
  },

  _addedShortcuts: false,
  _ensureShortcutsShown() {
    if (this._addedShortcuts) {
      return;
    }
    this._addedShortcuts = true;
    for (let button of this.mainView.querySelectorAll("toolbarbutton[key]")) {
      let keyId = button.getAttribute("key");
      let key = document.getElementById(keyId);
      if (!key) {
        continue;
      }
      button.setAttribute("shortcut", ShortcutUtils.prettifyShortcut(key));
    }
  },
};

XPCOMUtils.defineConstant(this, "PanelUI", PanelUI);

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale
 */
function getLocale() {
  return Services.locale.getAppLocaleAsLangTag();
}

function getNotificationFromElement(aElement) {
  // Need to find the associated notification object, which is a bit tricky
  // since it isn't associated with the element directly - this is kind of
  // gross and very dependent on the structure of the popupnotification
  // binding's content.
  let notificationEl;
  let parent = aElement;
  while (parent && (parent = aElement.ownerDocument.getBindingParent(parent))) {
    notificationEl = parent;
  }
  return notificationEl;
}
