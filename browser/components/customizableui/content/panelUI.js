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
      notificationPanel: "PanelUI-notification-popup",
      scroller: "PanelUI-contents-scroller",
      footer: "PanelUI-footer"
    };
  },

  _initialized: false,
  init() {
    for (let [k, v] of Object.entries(this.kElements)) {
      // Need to do fresh let-bindings per iteration
      let getKey = k;
      let id = v;
      this.__defineGetter__(getKey, function() {
        delete this[getKey];
        return this[getKey] = document.getElementById(id);
      });
    }

    this.notifications = [];
    this.menuButton.addEventListener("mousedown", this);
    this.menuButton.addEventListener("keypress", this);
    this._overlayScrollListenerBoundFn = this._overlayScrollListener.bind(this);

    Services.obs.addObserver(this, "fullscreen-nav-toolbox");
    Services.obs.addObserver(this, "panelUI-notification-main-action");
    Services.obs.addObserver(this, "panelUI-notification-dismissed");

    window.addEventListener("fullscreen", this);
    window.matchMedia("(-moz-overlay-scrollbars)").addListener(this._overlayScrollListenerBoundFn);
    CustomizableUI.addListener(this);

    for (let event of this.kEvents) {
      this.notificationPanel.addEventListener(event, this);
    }

    this._initialized = true;
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

  uninit() {
    for (let event of this.kEvents) {
      this.panel.removeEventListener(event, this);
      this.notificationPanel.removeEventListener(event, this);
    }

    Services.obs.removeObserver(this, "fullscreen-nav-toolbox");
    Services.obs.removeObserver(this, "panelUI-notification-main-action");
    Services.obs.removeObserver(this, "panelUI-notification-dismissed");

    window.removeEventListener("fullscreen", this);
    this.helpView.removeEventListener("ViewShowing", this._onHelpViewShow);
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
    return new Promise(resolve => {
      this.ensureReady().then(() => {
        if (this.panel.state == "open" ||
            document.documentElement.hasAttribute("customizing")) {
          resolve();
          return;
        }

        let editControlPlacement = CustomizableUI.getPlacementOfWidget("edit-controls");
        if (editControlPlacement && editControlPlacement.area == CustomizableUI.AREA_PANEL) {
          updateEditUIVisibility();
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

  showNotification(id, mainAction, secondaryActions = [], options = {}) {
    let notification = new PanelUINotification(id, mainAction, secondaryActions, options);
    let existingIndex = this.notifications.findIndex(n => n.id == id);
    if (existingIndex != -1) {
      this.notifications.splice(existingIndex, 1);
    }

    // We don't want to clobber doorhanger notifications just to show a badge,
    // so don't dismiss any of them and the badge will show once the doorhanger
    // gets resolved.
    if (!options.badgeOnly && !options.dismissed) {
      this.notifications.forEach(n => { n.dismissed = true; });
    }

    // Since notifications are generally somewhat pressing, the ideal case is that
    // we never have two notifications at once. However, in the event that we do,
    // it's more likely that the older notification has been sitting around for a
    // bit, and so we don't want to hide the new notification behind it. Thus,
    // we want our notifications to behave like a stack instead of a queue.
    this.notifications.unshift(notification);
    this._updateNotifications();
    return notification;
  },

  showBadgeOnlyNotification(id) {
    return this.showNotification(id, null, null, { badgeOnly: true });
  },

  removeNotification(id) {
    let notifications;
    if (typeof id == "string") {
      notifications = this.notifications.filter(n => n.id == id);
    } else {
      // If it's not a string, assume RegExp
      notifications = this.notifications.filter(n => id.test(n.id));
    }

    notifications.forEach(n => {
      this._removeNotification(n);
    });
    this._updateNotifications();
  },

  dismissNotification(id) {
    let notifications;
    if (typeof id == "string") {
      notifications = this.notifications.filter(n => n.id == id);
    } else {
      // If it's not a string, assume RegExp
      notifications = this.notifications.filter(n => id.test(n.id));
    }

    notifications.forEach(n => n.dismissed = true);
    this._updateNotifications();
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
        this._updateNotifications();
        break;
      case "panelUI-notification-main-action":
        if (subject != window) {
          this.removeNotification(status);
        }
        break;
      case "panelUI-notification-dismissed":
        if (subject != window) {
          this.dismissNotification(status);
        }
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
        // Fall through
      case "popupshown":
        // Fall through
      case "popuphiding":
        // Fall through
      case "popuphidden":
        this._updateNotifications();
        this._updatePanelButton(aEvent.target);
        break;
      case "mousedown":
        if (aEvent.button == 0)
          this.toggle(aEvent);
        break;
      case "keypress":
        this.toggle(aEvent);
        break;
      case "fullscreen":
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

  get activeNotification() {
    if (this.notifications.length > 0) {
      const doorhanger =
        this.notifications.find(n => !n.dismissed && !n.options.badgeOnly);
      return doorhanger || this.notifications[0];
    }

    return null;
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
    this._readyPromise = Task.spawn(function*() {
      if (!this._initialized) {
        yield new Promise(resolve => {
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
          (yield ScrollbarSampler.getSystemScrollbarWidth()) + "px";
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
        CustomizableUI.registerMenuPanel(this.contents);
      } else {
        this.beginBatchUpdate();
        try {
          CustomizableUI.registerMenuPanel(this.contents);
        } finally {
          this.endBatchUpdate();
        }
      }
      this._updateQuitTooltip();
      this.panel.hidden = false;
      this._isReady = true;
    }.bind(this)).then(null, Cu.reportError);

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
  showSubView: Task.async(function*(aViewId, aAnchor, aPlacementArea) {
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

    let container = aAnchor.closest("panelmultiview");
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
          let results = yield Promise.all(detail.blockers);
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
  }),

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

  onWidgetAfterDOMChange(aNode, aNextNode, aContainer, aWasRemoval) {
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

  _updateNotifications() {
    if (!this.notifications.length) {
      this._clearAllNotifications();
      this._hidePopup();
      return;
    }

    if (window.fullScreen && FullScreen.navToolboxHidden) {
      this._hidePopup();
      return;
    }

    let doorhangers =
      this.notifications.filter(n => !n.dismissed && !n.options.badgeOnly);

    if (this.panel.state == "showing" || this.panel.state == "open") {
      // If the menu is already showing, then we need to dismiss all notifications
      // since we don't want their doorhangers competing for attention
      doorhangers.forEach(n => { n.dismissed = true; })
      this._hidePopup();
      this._clearBadge();
      if (!this.notifications[0].options.badgeOnly) {
        this._showMenuItem(this.notifications[0]);
      }
    } else if (doorhangers.length > 0) {
      if (window.fullScreen) {
        this._hidePopup();
        this._showBadge(doorhangers[0]);
        this._showMenuItem(doorhangers[0]);
      } else {
        this._clearBadge();
        this._showNotificationPanel(doorhangers[0]);
      }
    } else {
      this._hidePopup();
      this._showBadge(this.notifications[0]);
      this._showMenuItem(this.notifications[0]);
    }
  },

  _showNotificationPanel(notification) {
    this._refreshNotificationPanel(notification);

    if (this.isNotificationPanelOpen) {
      return;
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
    this._clearMenuItems();
  },

  _refreshNotificationPanel(notification) {
    this._clearNotificationPanel();

    let popupnotificationID = this._getPopupId(notification);
    let popupnotification = document.getElementById(popupnotificationID);

    popupnotification.setAttribute("id", popupnotificationID);
    popupnotification.setAttribute("buttoncommand", "PanelUI._onNotificationButtonEvent(event, 'buttoncommand');");
    popupnotification.setAttribute("secondarybuttoncommand", "PanelUI._onNotificationButtonEvent(event, 'secondarybuttoncommand');");

    popupnotification.notification = notification;
    popupnotification.hidden = false;
  },

  _showBadge(notification) {
    let badgeStatus = this._getBadgeStatus(notification);
    this.menuButton.setAttribute("badge-status", badgeStatus);
  },

  // "Menu item" here refers to an item in the hamburger panel menu. They will
  // typically show up as a colored row near the bottom of the panel.
  _showMenuItem(notification) {
    this._clearMenuItems();

    let menuItemId = this._getMenuItemId(notification);
    let menuItem = document.getElementById(menuItemId);
    if (menuItem) {
      menuItem.notification = notification;
      menuItem.setAttribute("oncommand", "PanelUI._onNotificationMenuItemSelected(event)");
      menuItem.classList.add("PanelUI-notification-menu-item");
      menuItem.hidden = false;
      menuItem.fromPanelUINotifications = true;
    }
  },

  _clearBadge() {
    this.menuButton.removeAttribute("badge-status");
  },

  _clearMenuItems() {
    for (let child of this.footer.children) {
      if (child.fromPanelUINotifications) {
        child.notification = null;
        child.hidden = true;
      }
    }
  },

  _removeNotification(notification) {
    // This notification may already be removed, in which case let's just fail
    // silently.
    let notifications = this.notifications;
    if (!notifications)
      return;

    var index = notifications.indexOf(notification);
    if (index == -1)
      return;

    // Remove the notification
    notifications.splice(index, 1);
  },

  _onNotificationButtonEvent(event, type) {
    let notificationEl = getNotificationFromElement(event.originalTarget);

    if (!notificationEl)
      throw "PanelUI._onNotificationButtonEvent: couldn't find notification element";

    if (!notificationEl.notification)
      throw "PanelUI._onNotificationButtonEvent: couldn't find notification";

    let notification = notificationEl.notification;

    let action = notification.mainAction;

    if (type == "secondarybuttoncommand") {
      action = notification.secondaryActions[0];
    }

    let dismiss = true;
    if (action) {
      try {
        if (action === notification.mainAction) {
          action.callback(true);
          this._notify(notification.id, "main-action");
        } else {
          action.callback();
        }
      } catch (error) {
        Cu.reportError(error);
      }

      dismiss = action.dismiss;
    }

    if (dismiss) {
      notification.dismissed = true;
      this._notify(notification.id, "dismissed");
    } else {
      this._removeNotification(notification);
    }
    this._updateNotifications();
  },

  _onNotificationMenuItemSelected(event) {
    let target = event.originalTarget;
    if (!target.notification)
      throw "menucommand target has no associated action/notification";

    event.stopPropagation();

    try {
      target.notification.mainAction.callback(false);
      this._notify(target.notification.id, "main-action");
    } catch (error) {
      Cu.reportError(error);
    }

    this._removeNotification(target.notification);
    this._updateNotifications();
  },

  _getPopupId(notification) { return "PanelUI-" + notification.id + "-notification"; },

  _getBadgeStatus(notification) { return notification.id; },

  _getMenuItemId(notification) { return "PanelUI-" + notification.id + "-menu-item"; },

  _getPanelAnchor(candidate) {
    let iconAnchor =
      document.getAnonymousElementByAttribute(candidate, "class",
                                              "toolbarbutton-icon");
    return iconAnchor || candidate;
  },

  _notify(status, topic) {
    Services.obs.notifyObservers(window, "panelUI-notification-" + topic, status);
  }
};

XPCOMUtils.defineConstant(this, "PanelUI", PanelUI);

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale
 */
function getLocale() {
  return Services.locale.getAppLocaleAsLangTag();
}

function PanelUINotification(id, mainAction, secondaryActions = [], options = {}) {
  this.id = id;
  this.mainAction = mainAction;
  this.secondaryActions = secondaryActions;
  this.options = options;
  this.dismissed = this.options.dismissed || false;
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
