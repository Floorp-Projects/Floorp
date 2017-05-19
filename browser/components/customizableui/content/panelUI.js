/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ScrollbarSampler",
                                  "resource:///modules/ScrollbarSampler.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
                                  "resource://gre/modules/ShortcutUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppMenuNotifications",
                                  "resource://gre/modules/AppMenuNotifications.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "gPhotonStructure",
  "browser.photon.structure.enabled", false);

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
      contents: "PanelUI-contents",
      mainView: gPhotonStructure ? "appMenu-mainView" : "PanelUI-mainView",
      multiView: gPhotonStructure ? "appMenu-multiView" : "PanelUI-multiView",
      helpView: "PanelUI-helpView",
      menuButton: "PanelUI-menu-button",
      panel: gPhotonStructure ? "appMenu-popup" : "PanelUI-popup",
      notificationPanel: "appMenu-notification-popup",
      scroller: "PanelUI-contents-scroller",
      addonNotificationContainer: gPhotonStructure ? "appMenu-addon-banners" : "PanelUI-footer-addons",

      overflowFixedList: gPhotonStructure ? "widget-overflow-fixed-list" : "",
      overflowPanel: gPhotonStructure ? "widget-overflow" : "",
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

    window.addEventListener("fullscreen", this);
    window.addEventListener("activate", this);
    window.matchMedia("(-moz-overlay-scrollbars)").addListener(this._overlayScrollListenerBoundFn);
    CustomizableUI.addListener(this);

    for (let event of this.kEvents) {
      this.notificationPanel.addEventListener(event, this);
    }

    this._initPhotonPanel();
    Services.obs.notifyObservers(null, "appMenu-notifications-request", "refresh");

    this._initialized = true;
  },

  reinit() {
    this._removeEventListeners();
    // If the Photon pref changes, we need to re-init our element references.
    this._initElements();
    this._initPhotonPanel();
    delete this._readyPromise;
    this._isReady = false;
  },

  // We do this sync on init because in order to have the overflow button show up
  // we need to know whether anything is in the permanent panel area.
  _initPhotonPanel() {
    if (gPhotonStructure) {
      this.overflowFixedList.hidden = false;
      // Also unhide the separator. We use CSS to hide/show it based on the panel's content.
      this.overflowFixedList.previousSibling.hidden = false;
      CustomizableUI.registerMenuPanel(this.overflowFixedList, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
      this.navbar.setAttribute("photon-structure", "true");
      this.updateOverflowStatus();
    } else {
      this.navbar.removeAttribute("photon-structure");
    }
  },

  _initElements() {
    for (let [k, v] of Object.entries(this.kElements)) {
      if (!v) {
        continue;
      }
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

    window.removeEventListener("fullscreen", this);
    window.removeEventListener("activate", this);
    this.menuButton.removeEventListener("mousedown", this);
    this.menuButton.removeEventListener("keypress", this);
    window.matchMedia("(-moz-overlay-scrollbars)").removeListener(this._overlayScrollListenerBoundFn);
    CustomizableUI.removeListener(this);
    this._overlayScrollListenerBoundFn = null;
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
    if (gPhotonStructure) {
      this._ensureShortcutsShown();
    }
    return new Promise(resolve => {
      this.ensureReady().then(() => {
        if (this.panel.state == "open" ||
            document.documentElement.hasAttribute("customizing")) {
          resolve();
          return;
        }

        let personalBookmarksPlacement = CustomizableUI.getPlacementOfWidget("personal-bookmarks");
        if (personalBookmarksPlacement &&
            personalBookmarksPlacement.area == CustomizableUI.AREA_PANEL) {
          PlacesToolbarHelper.customizeChange();
        }

        let anchor;
        if (!aEvent ||
            aEvent.type == "command") {
          anchor = this.menuButton;
        } else {
          anchor = aEvent.target;
        }

        this.panel.addEventListener("popupshown", function() {
          resolve();
        }, {once: true});

        anchor = this._getPanelAnchor(anchor);
        this.panel.openPopup(anchor);
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
        this._adjustLabelsForAutoHyphens();
        updateEditUIVisibility();
        // Fall through
      case "popupshown":
        if (gPhotonStructure && aEvent.type == "popupshown") {
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
        if (gPhotonStructure && aEvent.type == "popuphidden") {
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
      case "fullscreen":
      case "activate":
        this._updateNotifications();
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
  ensureReady(aCustomizing = false) {
    if (this._readyPromise) {
      return this._readyPromise;
    }
    this._ensureEventListenersAdded();
    if (gPhotonStructure) {
      this.panel.hidden = false;
      this._readyPromise = Promise.resolve();
      this._isReady = true;
      return this._readyPromise;
    }
    this._readyPromise = (async () => {
      if (!this._initialized) {
        await new Promise(resolve => {
          let delayedStartupObserver = (aSubject, aTopic, aData) => {
            if (aSubject == window) {
              Services.obs.removeObserver(delayedStartupObserver, "browser-delayed-startup-finished");
              resolve();
            }
          };
          Services.obs.addObserver(delayedStartupObserver, "browser-delayed-startup-finished");
        });
      }

      this.contents.setAttributeNS("http://www.w3.org/XML/1998/namespace", "lang",
                                   getLocale());
      if (!this._scrollWidth) {
        // In order to properly center the contents of the panel, while ensuring
        // that we have enough space on either side to show a scrollbar, we have to
        // do a bit of hackery. In particular, we calculate a new width for the
        // scroller, based on the system scrollbar width.
        this._scrollWidth =
          (await ScrollbarSampler.getSystemScrollbarWidth()) + "px";
        let cstyle = window.getComputedStyle(this.scroller);
        let widthStr = cstyle.width;
        // Get the calculated padding on the left and right sides of
        // the scroller too. We'll use that in our final calculation so
        // that if a scrollbar appears, we don't have the contents right
        // up against the edge of the scroller.
        let paddingLeft = cstyle.paddingLeft;
        let paddingRight = cstyle.paddingRight;
        let calcStr = [widthStr, this._scrollWidth,
                       paddingLeft, paddingRight].join(" + ");
        this.scroller.style.width = "calc(" + calcStr + ")";
      }

      if (aCustomizing) {
        CustomizableUI.registerMenuPanel(this.contents, CustomizableUI.AREA_PANEL);
      } else {
        this.beginBatchUpdate();
        try {
          CustomizableUI.registerMenuPanel(this.contents, CustomizableUI.AREA_PANEL);
        } finally {
          this.endBatchUpdate();
        }
      }
      this._updateQuitTooltip();
      this.panel.hidden = false;
      this._isReady = true;
    })().then(null, Cu.reportError);

    return this._readyPromise;
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
   */
  async showSubView(aViewId, aAnchor, aPlacementArea, aAdopted = false) {
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

    let container = aAnchor.closest("panelmultiview,photonpanelmultiview");
    if (container) {
      container.showSubView(aViewId, aAnchor, null, aAdopted);
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
      document.getElementById(CustomizableUI.AREA_NAVBAR).appendChild(tempPanel);
      // If the view has a footer, set a convenience class on the panel.
      tempPanel.classList.toggle("cui-widget-panelWithFooter",
                                 viewNode.querySelector(".panel-subview-footer"));

      let multiView = document.createElement("panelmultiview");
      multiView.setAttribute("id", "customizationui-widget-multiview");
      multiView.setAttribute("nosubviews", "true");
      tempPanel.appendChild(multiView);
      multiView.setAttribute("mainViewIsSubView", "true");
      multiView.setMainView(viewNode);
      viewNode.classList.add("cui-widget-panelview");

      let viewShown = false;
      let panelRemover = () => {
        viewNode.classList.remove("cui-widget-panelview");
        if (viewShown) {
          CustomizableUI.removePanelCloseListeners(tempPanel);
          tempPanel.removeEventListener("popuphidden", panelRemover);

          let evt = new CustomEvent("ViewHiding", {detail: viewNode});
          viewNode.dispatchEvent(evt);
        }
        aAnchor.open = false;

        this.multiView.appendChild(viewNode);
        tempPanel.remove();
      };

      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
      let detail = {
        blockers: new Set(),
        addBlocker(aPromise) {
          this.blockers.add(aPromise);
        },
      };

      let evt = new CustomEvent("ViewShowing", { bubbles: true, cancelable: true, detail });
      viewNode.dispatchEvent(evt);

      let cancel = evt.defaultPrevented;
      if (detail.blockers.size) {
        try {
          let results = await Promise.all(detail.blockers);
          cancel = cancel || results.some(val => val === false);
        } catch (e) {
          Components.utils.reportError(e);
          cancel = true;
        }
      }

      if (cancel) {
        panelRemover();
        return;
      }

      viewShown = true;
      CustomizableUI.addPanelCloseListeners(tempPanel);
      tempPanel.addEventListener("popuphidden", panelRemover);

      let anchor = this._getPanelAnchor(aAnchor);

      if (aAnchor != anchor && aAnchor.id) {
        anchor.setAttribute("consumeanchor", aAnchor.id);
      }

      tempPanel.openPopup(anchor, "bottomcenter topright");
    }
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

  onPhotonChanged() {
    this.reinit();
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
    if (gPhotonStructure && aContainer == this.overflowFixedList) {
      this.updateOverflowStatus();
      return;
    }
    if (aContainer != this.contents) {
      return;
    }
    if (aWasRemoval) {
      aNode.removeAttribute("auto-hyphens");
    }
  },

  onWidgetBeforeDOMChange(aNode, aNextNode, aContainer, aIsRemoval) {
    if (aContainer != this.contents) {
      return;
    }
    if (!aIsRemoval &&
        (this.panel.state == "open" ||
         document.documentElement.hasAttribute("customizing"))) {
      this._adjustLabelsForAutoHyphens(aNode);
    }
  },

  /**
   * Signal that we're about to make a lot of changes to the contents of the
   * panels all at once. For performance, we ignore the mutations.
   */
  beginBatchUpdate() {
    this._ensureEventListenersAdded();
    this.multiView.ignoreMutations = true;
  },

  /**
   * Signal that we're done making bulk changes to the panel. We now pay
   * attention to mutations. This automatically synchronizes the multiview
   * container with whichever view is displayed if the panel is open.
   */
  endBatchUpdate(aReason) {
    this._ensureEventListenersAdded();
    this.multiView.ignoreMutations = false;
  },

  _adjustLabelsForAutoHyphens(aNode) {
    let toolbarButtons = aNode ? [aNode] :
                                 this.contents.querySelectorAll(".toolbarbutton-1");
    for (let node of toolbarButtons) {
      let label = node.getAttribute("label");
      if (!label) {
        continue;
      }
      if (label.includes("\u00ad")) {
        node.setAttribute("auto-hyphens", "off");
      } else {
        node.removeAttribute("auto-hyphens");
      }
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

    if (window.fullScreen && FullScreen.navToolboxHidden) {
      this._hidePopup();
      return;
    }

    let doorhangers =
      notifications.filter(n => !n.dismissed && !n.options.badgeOnly);

    if (this.panel.state == "showing" || this.panel.state == "open") {
      // If the menu is already showing, then we need to dismiss all notifications
      // since we don't want their doorhangers competing for attention
      doorhangers.forEach(n => { n.dismissed = true; })
      this._hidePopup();
      this._clearBadge();
      if (!notifications[0].options.badgeOnly) {
        this._showBannerItem(notifications[0]);
      }
    } else if (doorhangers.length > 0) {
      // Only show the doorhanger if the window is focused and not fullscreen
      if (window.fullScreen || Services.focus.activeWindow !== window) {
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
