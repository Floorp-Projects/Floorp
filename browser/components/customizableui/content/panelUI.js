/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ScrollbarSampler",
                                  "resource:///modules/ScrollbarSampler.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Pocket",
                                  "resource:///modules/Pocket.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
                                  "resource://gre/modules/ShortcutUtils.jsm");

/**
 * Maintains the state and dispatches events for the main menu panel.
 */

const PanelUI = {
  /** Panel events that we listen for. **/
  get kEvents() ["popupshowing", "popupshown", "popuphiding", "popuphidden"],
  /**
   * Used for lazily getting and memoizing elements from the document. Lazy
   * getters are set in init, and memoizing happens after the first retrieval.
   */
  get kElements() {
    return {
      contents: "PanelUI-contents",
      mainView: "PanelUI-mainView",
      multiView: "PanelUI-multiView",
      helpView: "PanelUI-helpView",
      menuButton: "PanelUI-menu-button",
      panel: "PanelUI-popup",
      scroller: "PanelUI-contents-scroller"
    };
  },

  _initialized: false,
  init: function() {
    for (let [k, v] of Iterator(this.kElements)) {
      // Need to do fresh let-bindings per iteration
      let getKey = k;
      let id = v;
      this.__defineGetter__(getKey, function() {
        delete this[getKey];
        return this[getKey] = document.getElementById(id);
      });
    }

    this.menuButton.addEventListener("mousedown", this);
    this.menuButton.addEventListener("keypress", this);
    this._overlayScrollListenerBoundFn = this._overlayScrollListener.bind(this);
    window.matchMedia("(-moz-overlay-scrollbars)").addListener(this._overlayScrollListenerBoundFn);
    CustomizableUI.addListener(this);
    this._initialized = true;
  },

  _eventListenersAdded: false,
  _ensureEventListenersAdded: function() {
    if (this._eventListenersAdded)
      return;
    this._addEventListeners();
  },

  _addEventListeners: function() {
    for (let event of this.kEvents) {
      this.panel.addEventListener(event, this);
    }

    this.helpView.addEventListener("ViewShowing", this._onHelpViewShow, false);
    this._eventListenersAdded = true;
  },

  uninit: function() {
    for (let event of this.kEvents) {
      this.panel.removeEventListener(event, this);
    }
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
  setMainView: function(aMainView) {
    this._ensureEventListenersAdded();
    this.multiView.setMainView(aMainView);
  },

  /**
   * Opens the menu panel if it's closed, or closes it if it's
   * open.
   *
   * @param aEvent the event that triggers the toggle.
   */
  toggle: function(aEvent) {
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
  show: function(aEvent) {
    let deferred = Promise.defer();

    this.ensureReady().then(() => {
      if (this.panel.state == "open" ||
          document.documentElement.hasAttribute("customizing")) {
        deferred.resolve();
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

      this.panel.addEventListener("popupshown", function onPopupShown() {
        this.removeEventListener("popupshown", onPopupShown);
        // As an optimization for the customize mode transition, we preload
        // about:customizing in the background once the menu panel is first
        // shown.
        gCustomizationTabPreloader.ensurePreloading();
        deferred.resolve();
      });

      let iconAnchor =
        document.getAnonymousElementByAttribute(anchor, "class",
                                                "toolbarbutton-icon");
      this.panel.openPopup(iconAnchor || anchor);
    });

    return deferred.promise;
  },

  /**
   * If the menu panel is being shown, hide it.
   */
  hide: function() {
    if (document.documentElement.hasAttribute("customizing")) {
      return;
    }

    this.panel.hidePopup();
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "popupshowing":
        this._adjustLabelsForAutoHyphens();
        // Fall through
      case "popupshown":
        // Fall through
      case "popuphiding":
        // Fall through
      case "popuphidden":
        this._updatePanelButton(aEvent.target);
        break;
      case "mousedown":
        if (aEvent.button == 0)
          this.toggle(aEvent);
        break;
      case "keypress":
        this.toggle(aEvent);
        break;
    }
  },

  get isReady() {
    return !!this._isReady;
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
  ensureReady: function(aCustomizing=false) {
    if (this._readyPromise) {
      return this._readyPromise;
    }
    this._readyPromise = Task.spawn(function() {
      if (!this._initialized) {
        let delayedStartupDeferred = Promise.defer();
        let delayedStartupObserver = (aSubject, aTopic, aData) => {
          if (aSubject == window) {
            Services.obs.removeObserver(delayedStartupObserver, "browser-delayed-startup-finished");
            delayedStartupDeferred.resolve();
          }
        };
        Services.obs.addObserver(delayedStartupObserver, "browser-delayed-startup-finished", false);
        yield delayedStartupDeferred.promise;
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
  showMainView: function() {
    this._ensureEventListenersAdded();
    this.multiView.showMainView();
  },

  /**
   * Switch the panel to the help view if it's not already
   * in that view.
   */
  showHelpView: function(aAnchor) {
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
  showSubView: function(aViewId, aAnchor, aPlacementArea) {
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

    if (aPlacementArea == CustomizableUI.AREA_PANEL) {
      this.multiView.showSubView(aViewId, aAnchor);
    } else if (!aAnchor.open) {
      aAnchor.open = true;
      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
      let evt = document.createEvent("CustomEvent");
      evt.initCustomEvent("ViewShowing", true, true, viewNode);
      viewNode.dispatchEvent(evt);
      if (evt.defaultPrevented) {
        return;
      }

      let tempPanel = document.createElement("panel");
      tempPanel.setAttribute("type", "arrow");
      tempPanel.setAttribute("id", "customizationui-widget-panel");
      tempPanel.setAttribute("class", "cui-widget-panel");
      tempPanel.setAttribute("viewId", aViewId);
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
      CustomizableUI.addPanelCloseListeners(tempPanel);

      let panelRemover = function() {
        tempPanel.removeEventListener("popuphidden", panelRemover);
        viewNode.classList.remove("cui-widget-panelview");
        CustomizableUI.removePanelCloseListeners(tempPanel);
        let evt = new CustomEvent("ViewHiding", {detail: viewNode});
        viewNode.dispatchEvent(evt);
        aAnchor.open = false;

        this.multiView.appendChild(viewNode);
        tempPanel.parentElement.removeChild(tempPanel);
      }.bind(this);
      tempPanel.addEventListener("popuphidden", panelRemover);

      let iconAnchor =
        document.getAnonymousElementByAttribute(aAnchor, "class",
                                                "toolbarbutton-icon");

      if (iconAnchor && aAnchor.id) {
        iconAnchor.setAttribute("consumeanchor", aAnchor.id);
      }
      tempPanel.openPopup(iconAnchor || aAnchor, "bottomcenter topright");
    }
  },

  /**
   * NB: The enable- and disableSingleSubviewPanelAnimations methods only
   * affect the hiding/showing animations of single-subview panels (tempPanel
   * in the showSubView method).
   */
  disableSingleSubviewPanelAnimations: function() {
    this._disableAnimations = true;
  },

  enableSingleSubviewPanelAnimations: function() {
    this._disableAnimations = false;
  },

  onWidgetAfterDOMChange: function(aNode, aNextNode, aContainer, aWasRemoval) {
    if (aContainer != this.contents) {
      return;
    }
    if (aWasRemoval) {
      aNode.removeAttribute("auto-hyphens");
    }
  },

  onWidgetBeforeDOMChange: function(aNode, aNextNode, aContainer, aIsRemoval) {
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
  beginBatchUpdate: function() {
    this._ensureEventListenersAdded();
    this.multiView.ignoreMutations = true;
  },

  /**
   * Signal that we're done making bulk changes to the panel. We now pay
   * attention to mutations. This automatically synchronizes the multiview
   * container with whichever view is displayed if the panel is open.
   */
  endBatchUpdate: function(aReason) {
    this._ensureEventListenersAdded();
    this.multiView.ignoreMutations = false;
  },

  _adjustLabelsForAutoHyphens: function(aNode) {
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
  _updatePanelButton: function() {
    this.menuButton.open = this.panel.state == "open" ||
                           this.panel.state == "showing";
  },

  _onHelpViewShow: function(aEvent) {
    // Call global menu setup function
    buildHelpMenu();

    let helpMenu = document.getElementById("menu_HelpPopup");
    let items = this.getElementsByTagName("vbox")[0];
    let attrs = ["oncommand", "onclick", "label", "key", "disabled"];
    let NSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    // Remove all buttons from the view
    while (items.firstChild) {
      items.removeChild(items.firstChild);
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

  _updateQuitTooltip: function() {
#ifndef XP_WIN
#ifdef XP_MACOSX
    let tooltipId = "quit-button.tooltiptext.mac";
#else
    let tooltipId = "quit-button.tooltiptext.linux2";
#endif
    let brands = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    let stringArgs = [brands.GetStringFromName("brandShortName")];

    let key = document.getElementById("key_quitApplication");
    stringArgs.push(ShortcutUtils.prettifyShortcut(key));
    let tooltipString = CustomizableUI.getLocalizedProperty({x: tooltipId}, "x", stringArgs);
    let quitButton = document.getElementById("PanelUI-quit");
    quitButton.setAttribute("tooltiptext", tooltipString);
#endif
  },

  _overlayScrollListenerBoundFn: null,
  _overlayScrollListener: function(aMQL) {
    ScrollbarSampler.resetSystemScrollbarWidth();
    this._scrollWidth = null;
  },
};

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale or "en-US" if none is selected
 */
function getLocale() {
  try {
    let chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"]
                           .getService(Ci.nsIXULChromeRegistry);
    return chromeRegistry.getSelectedLocale("browser");
  } catch (ex) {
    return "en-US";
  }
}
