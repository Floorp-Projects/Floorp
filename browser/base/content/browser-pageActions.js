/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineModuleGetter(
  this,
  "SearchUIUtils",
  "resource:///modules/SearchUIUtils.jsm"
);

var BrowserPageActions = {
  _panelNode: null,
  /**
   * The main page action button in the urlbar (DOM node)
   */
  get mainButtonNode() {
    delete this.mainButtonNode;
    return (this.mainButtonNode = document.getElementById("pageActionButton"));
  },

  /**
   * The main page action panel DOM node (DOM node)
   */
  get panelNode() {
    // Lazy load the page action panel the first time we need to display it
    if (!this._panelNode) {
      this.initializePanel();
    }
    delete this.panelNode;
    return (this.panelNode = this._panelNode);
  },

  /**
   * The panelmultiview node in the main page action panel (DOM node)
   */
  get multiViewNode() {
    delete this.multiViewNode;
    return (this.multiViewNode = document.getElementById(
      "pageActionPanelMultiView"
    ));
  },

  /**
   * The main panelview node in the main page action panel (DOM node)
   */
  get mainViewNode() {
    delete this.mainViewNode;
    return (this.mainViewNode = document.getElementById(
      "pageActionPanelMainView"
    ));
  },

  /**
   * The vbox body node in the main panelview node (DOM node)
   */
  get mainViewBodyNode() {
    delete this.mainViewBodyNode;
    return (this.mainViewBodyNode = this.mainViewNode.querySelector(
      ".panel-subview-body"
    ));
  },

  /**
   * Inits.  Call to init.
   */
  init() {
    this.placeAllActionsInUrlbar();
    this._onPanelShowing = this._onPanelShowing.bind(this);
  },

  _onPanelShowing() {
    this.initializePanel();
    for (let action of PageActions.actionsInPanel(window)) {
      let buttonNode = this.panelButtonNodeForActionID(action.id);
      action.onShowingInPanel(buttonNode);
    }
  },

  placeLazyActionsInPanel() {
    let actions = this._actionsToLazilyPlaceInPanel;
    this._actionsToLazilyPlaceInPanel = [];
    for (let action of actions) {
      this._placeActionInPanelNow(action);
    }
  },

  // Actions placed in the panel aren't actually placed until the panel is
  // subsequently opened.
  _actionsToLazilyPlaceInPanel: [],

  /**
   * Places all registered actions in the urlbar.
   */
  placeAllActionsInUrlbar() {
    let urlbarActions = PageActions.actionsInUrlbar(window);
    for (let action of urlbarActions) {
      this.placeActionInUrlbar(action);
    }
    this._updateMainButtonAttributes();
  },

  /**
   * Initializes the panel if necessary.
   */
  initializePanel() {
    // Lazy load the page action panel the first time we need to display it
    if (!this._panelNode) {
      let template = document.getElementById("pageActionPanelTemplate");
      template.replaceWith(template.content);
      this._panelNode = document.getElementById("pageActionPanel");
      this._panelNode.addEventListener("popupshowing", this._onPanelShowing);
      this._panelNode.addEventListener("popuphiding", () => {
        this.mainButtonNode.removeAttribute("open");
      });
    }

    for (let action of PageActions.actionsInPanel(window)) {
      this.placeActionInPanel(action);
    }
    this.placeLazyActionsInPanel();
  },

  /**
   * Adds or removes as necessary DOM nodes for the given action.
   *
   * @param  action (PageActions.Action, required)
   *         The action to place.
   */
  placeAction(action) {
    this.placeActionInPanel(action);
    this.placeActionInUrlbar(action);
    this._updateMainButtonAttributes();
  },

  /**
   * Adds or removes as necessary DOM nodes for the action in the panel.
   *
   * @param  action (PageActions.Action, required)
   *         The action to place.
   */
  placeActionInPanel(action) {
    if (this._panelNode && this.panelNode.state != "closed") {
      this._placeActionInPanelNow(action);
    } else {
      // Lazily place the action in the panel the next time it opens.
      this._actionsToLazilyPlaceInPanel.push(action);
    }
  },

  _placeActionInPanelNow(action) {
    if (action.shouldShowInPanel(window)) {
      this._addActionToPanel(action);
    } else {
      this._removeActionFromPanel(action);
    }
  },

  _addActionToPanel(action) {
    let id = this.panelButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);
    if (node) {
      return;
    }
    this._maybeNotifyBeforePlacedInWindow(action);
    node = this._makePanelButtonNodeForAction(action);
    node.id = id;
    let insertBeforeNode = this._getNextNode(action, false);
    this.mainViewBodyNode.insertBefore(node, insertBeforeNode);
    this.updateAction(action, null, {
      panelNode: node,
    });
    this._updateActionDisabledInPanel(action, node);
    action.onPlacedInPanel(node);
    this._addOrRemoveSeparatorsInPanel();
  },

  _removeActionFromPanel(action) {
    let lazyIndex = this._actionsToLazilyPlaceInPanel.findIndex(
      a => a.id == action.id
    );
    if (lazyIndex >= 0) {
      this._actionsToLazilyPlaceInPanel.splice(lazyIndex, 1);
    }
    let node = this.panelButtonNodeForActionID(action.id);
    if (!node) {
      return;
    }
    node.remove();
    if (action.getWantsSubview(window)) {
      let panelViewNodeID = this._panelViewNodeIDForActionID(action.id, false);
      let panelViewNode = document.getElementById(panelViewNodeID);
      if (panelViewNode) {
        panelViewNode.remove();
      }
    }
    this._addOrRemoveSeparatorsInPanel();
  },

  _addOrRemoveSeparatorsInPanel() {
    let actions = PageActions.actionsInPanel(window);
    let ids = [
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
      PageActions.ACTION_ID_TRANSIENT_SEPARATOR,
    ];
    for (let id of ids) {
      let sep = actions.find(a => a.id == id);
      if (sep) {
        this._addActionToPanel(sep);
      } else {
        let node = this.panelButtonNodeForActionID(id);
        if (node) {
          node.remove();
        }
      }
    }
  },

  _updateMainButtonAttributes() {
    this.mainButtonNode.toggleAttribute(
      "multiple-children",
      PageActions.actions.length > 1
    );
  },

  /**
   * Returns the node before which an action's node should be inserted.
   *
   * @param  action (PageActions.Action, required)
   *         The action that will be inserted.
   * @param  forUrlbar (bool, required)
   *         True if you're inserting into the urlbar, false if you're inserting
   *         into the panel.
   * @return (DOM node, maybe null) The DOM node before which to insert the
   *         given action.  Null if the action should be inserted at the end.
   */
  _getNextNode(action, forUrlbar) {
    let actions = forUrlbar
      ? PageActions.actionsInUrlbar(window)
      : PageActions.actionsInPanel(window);
    let index = actions.findIndex(a => a.id == action.id);
    if (index < 0) {
      return null;
    }
    for (let i = index + 1; i < actions.length; i++) {
      let node = forUrlbar
        ? this.urlbarButtonNodeForActionID(actions[i].id)
        : this.panelButtonNodeForActionID(actions[i].id);
      if (node) {
        return node;
      }
    }
    return null;
  },

  _maybeNotifyBeforePlacedInWindow(action) {
    if (!this._isActionPlacedInWindow(action)) {
      action.onBeforePlacedInWindow(window);
    }
  },

  _isActionPlacedInWindow(action) {
    if (this.panelButtonNodeForActionID(action.id)) {
      return true;
    }
    let urlbarNode = this.urlbarButtonNodeForActionID(action.id);
    return urlbarNode && !urlbarNode.hidden;
  },

  _makePanelButtonNodeForAction(action) {
    if (action.__isSeparator) {
      let node = document.createXULElement("toolbarseparator");
      return node;
    }
    let buttonNode = document.createXULElement("toolbarbutton");
    buttonNode.classList.add(
      "subviewbutton",
      "subviewbutton-iconic",
      "pageAction-panel-button"
    );
    if (action.isBadged) {
      buttonNode.setAttribute("badged", "true");
    }
    buttonNode.setAttribute("actionid", action.id);
    buttonNode.addEventListener("command", event => {
      this.doCommandForAction(action, event, buttonNode);
    });
    return buttonNode;
  },

  _makePanelViewNodeForAction(action, forUrlbar) {
    let panelViewNode = document.createXULElement("panelview");
    panelViewNode.id = this._panelViewNodeIDForActionID(action.id, forUrlbar);
    panelViewNode.classList.add("PanelUI-subView");
    let bodyNode = document.createXULElement("vbox");
    bodyNode.id = panelViewNode.id + "-body";
    bodyNode.classList.add("panel-subview-body");
    panelViewNode.appendChild(bodyNode);
    return panelViewNode;
  },

  /**
   * Shows or hides a panel for an action.  You can supply your own panel;
   * otherwise one is created.
   *
   * @param  action (PageActions.Action, required)
   *         The action for which to toggle the panel.  If the action is in the
   *         urlbar, then the panel will be anchored to it.  Otherwise, a
   *         suitable anchor will be used.
   * @param  panelNode (DOM node, optional)
   *         The panel to use.  This method takes a hands-off approach with
   *         regard to your panel in terms of attributes, styling, etc.
   * @param  event (DOM event, optional)
   *         The event which triggered this panel.
   */
  togglePanelForAction(action, panelNode = null, event = null) {
    let aaPanelNode = this.activatedActionPanelNode;
    if (panelNode) {
      // Note that this particular code path will not prevent the panel from
      // opening later if PanelMultiView.showPopup was called but the panel has
      // not been opened yet.
      if (panelNode.state != "closed") {
        PanelMultiView.hidePopup(panelNode);
        return;
      }
      if (aaPanelNode) {
        PanelMultiView.hidePopup(aaPanelNode);
      }
    } else if (aaPanelNode) {
      PanelMultiView.hidePopup(aaPanelNode);
      return;
    } else {
      panelNode = this._makeActivatedActionPanelForAction(action);
    }

    // Hide the main panel before showing the action's panel.
    PanelMultiView.hidePopup(this.panelNode);

    let anchorNode = this.panelAnchorNodeForAction(action);
    anchorNode.setAttribute("open", "true");
    panelNode.addEventListener(
      "popuphiding",
      () => {
        anchorNode.removeAttribute("open");
      },
      { once: true }
    );

    PanelMultiView.openPopup(panelNode, anchorNode, {
      position: "bottomcenter topright",
      triggerEvent: event,
    }).catch(Cu.reportError);
  },

  _makeActivatedActionPanelForAction(action) {
    let panelNode = document.createXULElement("panel");
    panelNode.id = this._activatedActionPanelID;
    panelNode.classList.add("cui-widget-panel", "panel-no-padding");
    panelNode.setAttribute("actionID", action.id);
    panelNode.setAttribute("role", "group");
    panelNode.setAttribute("type", "arrow");
    panelNode.setAttribute("flip", "slide");
    panelNode.setAttribute("noautofocus", "true");
    panelNode.setAttribute("tabspecific", "true");

    let panelViewNode = null;
    let iframeNode = null;

    if (action.getWantsSubview(window)) {
      let multiViewNode = document.createXULElement("panelmultiview");
      panelViewNode = this._makePanelViewNodeForAction(action, true);
      multiViewNode.setAttribute("mainViewId", panelViewNode.id);
      multiViewNode.appendChild(panelViewNode);
      panelNode.appendChild(multiViewNode);
    } else if (action.wantsIframe) {
      iframeNode = document.createXULElement("iframe");
      iframeNode.setAttribute("type", "content");
      panelNode.appendChild(iframeNode);
    }

    let popupSet = document.getElementById("mainPopupSet");
    popupSet.appendChild(panelNode);
    panelNode.addEventListener(
      "popuphidden",
      () => {
        PanelMultiView.removePopup(panelNode);
      },
      { once: true }
    );

    if (iframeNode) {
      panelNode.addEventListener(
        "popupshowing",
        () => {
          action.onIframeShowing(iframeNode, panelNode);
        },
        { once: true }
      );
      panelNode.addEventListener(
        "popupshown",
        () => {
          iframeNode.focus();
        },
        { once: true }
      );
      panelNode.addEventListener(
        "popuphiding",
        () => {
          action.onIframeHiding(iframeNode, panelNode);
        },
        { once: true }
      );
      panelNode.addEventListener(
        "popuphidden",
        () => {
          action.onIframeHidden(iframeNode, panelNode);
        },
        { once: true }
      );
    }

    if (panelViewNode) {
      action.onSubviewPlaced(panelViewNode);
      panelNode.addEventListener(
        "popupshowing",
        () => {
          action.onSubviewShowing(panelViewNode);
        },
        { once: true }
      );
    }

    return panelNode;
  },

  /**
   * Returns the node in the urlbar to which popups for the given action should
   * be anchored.  If the action is null, a sensible anchor is returned.
   *
   * @param  action (PageActions.Action, optional)
   *         The action you want to anchor.
   * @param  event (DOM event, optional)
   *         This is used to display the feedback panel on the right node when
   *         the command can be invoked from both the main panel and another
   *         location, such as an activated action panel or a button.
   * @return (DOM node) The node to which the action should be anchored.
   */
  panelAnchorNodeForAction(action, event) {
    if (event && event.target.closest("panel") == this.panelNode) {
      return this.mainButtonNode;
    }

    // Try each of the following nodes in order, using the first that's visible.
    let potentialAnchorNodeIDs = [
      action && action.anchorIDOverride,
      action && this.urlbarButtonNodeIDForActionID(action.id),
      this.mainButtonNode.id,
      "identity-icon",
      "urlbar-search-button",
    ];
    for (let id of potentialAnchorNodeIDs) {
      if (id) {
        let node = document.getElementById(id);
        if (node && !node.hidden) {
          let bounds = window.windowUtils.getBoundsWithoutFlushing(node);
          if (bounds.height > 0 && bounds.width > 0) {
            return node;
          }
        }
      }
    }
    let id = action ? action.id : "<no action>";
    throw new Error(`PageActions: No anchor node for ${id}`);
  },

  get activatedActionPanelNode() {
    return document.getElementById(this._activatedActionPanelID);
  },

  get _activatedActionPanelID() {
    return "pageActionActivatedActionPanel";
  },

  /**
   * Adds or removes as necessary a DOM node for the given action in the urlbar.
   *
   * @param  action (PageActions.Action, required)
   *         The action to place.
   */
  placeActionInUrlbar(action) {
    let id = this.urlbarButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);

    if (!action.shouldShowInUrlbar(window)) {
      if (node) {
        if (action.__urlbarNodeInMarkup) {
          node.hidden = true;
        } else {
          node.remove();
        }
      }
      return;
    }

    let newlyPlaced = false;
    if (action.__urlbarNodeInMarkup) {
      this._maybeNotifyBeforePlacedInWindow(action);
      // Allow the consumer to add the node in response to the
      // onBeforePlacedInWindow notification.
      node = document.getElementById(id);
      if (!node) {
        return;
      }
      newlyPlaced = node.hidden;
      node.hidden = false;
    } else if (!node) {
      newlyPlaced = true;
      this._maybeNotifyBeforePlacedInWindow(action);
      node = this._makeUrlbarButtonNode(action);
      node.id = id;
    }

    if (!newlyPlaced) {
      return;
    }

    let insertBeforeNode = this._getNextNode(action, true);
    this.mainButtonNode.parentNode.insertBefore(node, insertBeforeNode);
    this.updateAction(action, null, {
      urlbarNode: node,
    });
    action.onPlacedInUrlbar(node);
  },

  _makeUrlbarButtonNode(action) {
    let buttonNode = document.createXULElement("image");
    buttonNode.classList.add("urlbar-icon", "urlbar-page-action");
    buttonNode.setAttribute("actionid", action.id);
    buttonNode.setAttribute("role", "button");
    let commandHandler = event => {
      this.doCommandForAction(action, event, buttonNode);
    };
    buttonNode.addEventListener("click", commandHandler);
    buttonNode.addEventListener("keypress", commandHandler);
    return buttonNode;
  },

  /**
   * Removes all the DOM nodes of the given action.
   *
   * @param  action (PageActions.Action, required)
   *         The action to remove.
   */
  removeAction(action) {
    this._removeActionFromPanel(action);
    this._removeActionFromUrlbar(action);
    action.onRemovedFromWindow(window);
    this._updateMainButtonAttributes();
  },

  _removeActionFromUrlbar(action) {
    let node = this.urlbarButtonNodeForActionID(action.id);
    if (node) {
      node.remove();
    }
  },

  /**
   * Updates the DOM nodes of an action to reflect either a changed property or
   * all properties.
   *
   * @param  action (PageActions.Action, required)
   *         The action to update.
   * @param  propertyName (string, optional)
   *         The name of the property to update.  If not given, then DOM nodes
   *         will be updated to reflect the current values of all properties.
   * @param  opts (object, optional)
   *         - panelNode: The action's node in the panel to update.
   *         - urlbarNode: The action's node in the urlbar to update.
   *         - value: If a property name is passed, this argument may contain
   *           its current value, in order to prevent a further look-up.
   */
  updateAction(action, propertyName = null, opts = {}) {
    let anyNodeGiven = "panelNode" in opts || "urlbarNode" in opts;
    let panelNode = anyNodeGiven
      ? opts.panelNode || null
      : this.panelButtonNodeForActionID(action.id);
    let urlbarNode = anyNodeGiven
      ? opts.urlbarNode || null
      : this.urlbarButtonNodeForActionID(action.id);
    let value = opts.value || undefined;
    if (propertyName) {
      this[this._updateMethods[propertyName]](
        action,
        panelNode,
        urlbarNode,
        value
      );
    } else {
      for (let name of ["iconURL", "title", "tooltip", "wantsSubview"]) {
        this[this._updateMethods[name]](action, panelNode, urlbarNode, value);
      }
    }
  },

  _updateMethods: {
    disabled: "_updateActionDisabled",
    iconURL: "_updateActionIconURL",
    title: "_updateActionLabeling",
    tooltip: "_updateActionTooltip",
    wantsSubview: "_updateActionWantsSubview",
  },

  _updateActionDisabled(
    action,
    panelNode,
    urlbarNode,
    disabled = action.getDisabled(window)
  ) {
    if (action.__transient) {
      this.placeActionInPanel(action);
    } else {
      this._updateActionDisabledInPanel(action, panelNode, disabled);
    }
    this.placeActionInUrlbar(action);
  },

  _updateActionDisabledInPanel(
    action,
    panelNode,
    disabled = action.getDisabled(window)
  ) {
    if (panelNode) {
      if (disabled) {
        panelNode.setAttribute("disabled", "true");
      } else {
        panelNode.removeAttribute("disabled");
      }
    }
  },

  _updateActionIconURL(
    action,
    panelNode,
    urlbarNode,
    properties = action.getIconProperties(window)
  ) {
    for (let [prop, value] of Object.entries(properties)) {
      if (panelNode) {
        panelNode.style.setProperty(prop, value);
      }
      if (urlbarNode) {
        urlbarNode.style.setProperty(prop, value);
      }
    }
  },

  _updateActionLabeling(
    action,
    panelNode,
    urlbarNode,
    title = action.getTitle(window)
  ) {
    let tabCount = gBrowser.selectedTabs.length;
    if (panelNode) {
      if (action.panelFluentID) {
        document.l10n.setAttributes(panelNode, action.panelFluentID, {
          tabCount,
        });
      } else {
        panelNode.setAttribute("label", title);
      }
    }
    if (urlbarNode) {
      // Some actions (e.g. Save Page to Pocket) have a wrapper node with the
      // actual controls inside that wrapper. The wrapper is semantically
      // meaningless, so it doesn't get reflected in the accessibility tree.
      // In these cases, we don't want to set aria-label because that will
      // force the element to be exposed to accessibility.
      if (urlbarNode.nodeName != "hbox") {
        urlbarNode.setAttribute("aria-label", title);
      }
      // tooltiptext falls back to the title, so update it too if necessary.
      let tooltip = action.getTooltip(window);
      if (!tooltip) {
        if (action.urlbarFluentID) {
          document.l10n.setAttributes(urlbarNode, action.urlbarFluentID, {
            tabCount,
          });
        } else {
          urlbarNode.setAttribute("tooltiptext", title);
        }
      }
    }
  },

  _updateActionTooltip(
    action,
    panelNode,
    urlbarNode,
    tooltip = action.getTooltip(window)
  ) {
    if (urlbarNode) {
      if (!tooltip) {
        tooltip = action.getTitle(window);
      }
      if (tooltip) {
        urlbarNode.setAttribute("tooltiptext", tooltip);
      }
    }
  },

  _updateActionWantsSubview(
    action,
    panelNode,
    urlbarNode,
    wantsSubview = action.getWantsSubview(window)
  ) {
    if (!panelNode) {
      return;
    }
    let panelViewID = this._panelViewNodeIDForActionID(action.id, false);
    let panelViewNode = document.getElementById(panelViewID);
    panelNode.classList.toggle("subviewbutton-nav", wantsSubview);
    if (!wantsSubview) {
      if (panelViewNode) {
        panelViewNode.remove();
      }
      return;
    }
    if (!panelViewNode) {
      panelViewNode = this._makePanelViewNodeForAction(action, false);
      this.multiViewNode.appendChild(panelViewNode);
      action.onSubviewPlaced(panelViewNode);
    }
  },

  doCommandForAction(action, event, buttonNode) {
    if (event && event.type == "click" && event.button != 0) {
      return;
    }
    if (event && event.type == "keypress") {
      if (event.key != " " && event.key != "Enter") {
        return;
      }
      event.stopPropagation();
    }
    // If we're in the panel, open a subview inside the panel:
    // Note that we can't use this.panelNode.contains(buttonNode) here
    // because of XBL boundaries breaking Element.contains.
    if (
      action.getWantsSubview(window) &&
      buttonNode &&
      buttonNode.closest("panel") == this.panelNode
    ) {
      let panelViewNodeID = this._panelViewNodeIDForActionID(action.id, false);
      let panelViewNode = document.getElementById(panelViewNodeID);
      action.onSubviewShowing(panelViewNode);
      this.multiViewNode.showSubView(panelViewNode, buttonNode);
      return;
    }
    // Otherwise, hide the main popup in case it was open:
    PanelMultiView.hidePopup(this.panelNode);

    let aaPanelNode = this.activatedActionPanelNode;
    if (!aaPanelNode || aaPanelNode.getAttribute("actionID") != action.id) {
      action.onCommand(event, buttonNode);
    }
    if (action.getWantsSubview(window) || action.wantsIframe) {
      this.togglePanelForAction(action, null, event);
    }
  },

  /**
   * Returns the action for a node.
   *
   * @param  node (DOM node, required)
   *         A button DOM node, either one that's shown in the page action panel
   *         or the urlbar.
   * @return (PageAction.Action) If the node has a related action and the action
   *         is not a separator, then the action is returned.  Otherwise null is
   *         returned.
   */
  actionForNode(node) {
    if (!node) {
      return null;
    }
    let actionID = this._actionIDForNodeID(node.id);
    let action = PageActions.actionForID(actionID);
    if (!action) {
      // The given node may be an ancestor of a node corresponding to an action,
      // like how #star-button is contained in #star-button-box, the latter
      // being the bookmark action's node.  Look up the ancestor chain.
      for (let n = node.parentNode; n && !action; n = n.parentNode) {
        if (n.id == "page-action-buttons" || n.localName == "panelview") {
          // We reached the page-action-buttons or panelview container.
          // Stop looking; no acton was found.
          break;
        }
        actionID = this._actionIDForNodeID(n.id);
        action = PageActions.actionForID(actionID);
      }
    }
    return action && !action.__isSeparator ? action : null;
  },

  /**
   * The given action's top-level button in the main panel.
   *
   * @param  actionID (string, required)
   *         The action ID.
   * @return (DOM node) The action's button in the main panel.
   */
  panelButtonNodeForActionID(actionID) {
    return document.getElementById(this.panelButtonNodeIDForActionID(actionID));
  },

  /**
   * The ID of the given action's top-level button in the main panel.
   *
   * @param  actionID (string, required)
   *         The action ID.
   * @return (string) The ID of the action's button in the main panel.
   */
  panelButtonNodeIDForActionID(actionID) {
    return `pageAction-panel-${actionID}`;
  },

  /**
   * The given action's button in the urlbar.
   *
   * @param  actionID (string, required)
   *         The action ID.
   * @return (DOM node) The action's urlbar button node.
   */
  urlbarButtonNodeForActionID(actionID) {
    return document.getElementById(
      this.urlbarButtonNodeIDForActionID(actionID)
    );
  },

  /**
   * The ID of the given action's button in the urlbar.
   *
   * @param  actionID (string, required)
   *         The action ID.
   * @return (string) The ID of the action's urlbar button node.
   */
  urlbarButtonNodeIDForActionID(actionID) {
    let action = PageActions.actionForID(actionID);
    if (action && action.urlbarIDOverride) {
      return action.urlbarIDOverride;
    }
    return `pageAction-urlbar-${actionID}`;
  },

  // The ID of the given action's panelview.
  _panelViewNodeIDForActionID(actionID, forUrlbar) {
    let placementID = forUrlbar ? "urlbar" : "panel";
    return `pageAction-${placementID}-${actionID}-subview`;
  },

  // The ID of the action corresponding to the given top-level button in the
  // panel or button in the urlbar.
  _actionIDForNodeID(nodeID) {
    if (!nodeID) {
      return null;
    }
    let match = nodeID.match(/^pageAction-(?:panel|urlbar)-(.+)$/);
    if (match) {
      return match[1];
    }
    // Check all the urlbar ID overrides.
    for (let action of PageActions.actions) {
      if (action.urlbarIDOverride && action.urlbarIDOverride == nodeID) {
        return action.id;
      }
    }
    return null;
  },

  /**
   * Call this when the main page action button in the urlbar is activated.
   *
   * @param  event (DOM event, required)
   *         The click or whatever event.
   */
  mainButtonClicked(event) {
    event.stopPropagation();
    if (
      // On mac, ctrl-click will send a context menu event from the widget, so
      // we don't want to bring up the panel when ctrl key is pressed.
      (event.type == "mousedown" &&
        (event.button != 0 ||
          (AppConstants.platform == "macosx" && event.ctrlKey))) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return;
    }

    // If the activated-action panel is open and anchored to the main button,
    // close it.
    let panelNode = this.activatedActionPanelNode;
    if (panelNode && panelNode.anchorNode.id == this.mainButtonNode.id) {
      PanelMultiView.hidePopup(panelNode);
      return;
    }

    if (this.panelNode.state == "open") {
      PanelMultiView.hidePopup(this.panelNode);
    } else if (this.panelNode.state == "closed") {
      this.showPanel(event);
    }
  },

  /**
   * Show the page action panel
   *
   * @param  event (DOM event, optional)
   *         The event that triggers showing the panel. (such as a mouse click,
   *         if the user clicked something to open the panel)
   */
  showPanel(event = null) {
    this.panelNode.hidden = false;
    this.mainButtonNode.setAttribute("open", "true");
    PanelMultiView.openPopup(this.panelNode, this.mainButtonNode, {
      position: "bottomcenter topright",
      triggerEvent: event,
    }).catch(Cu.reportError);
  },

  /**
   * Call this on the context menu's popupshowing event.
   *
   * @param  event (DOM event, required)
   *         The popupshowing event.
   * @param  popup (DOM node, required)
   *         The context menu popup DOM node.
   */
  async onContextMenuShowing(event, popup) {
    if (event.target != popup) {
      return;
    }

    let action = this.actionForNode(popup.triggerNode);
    if (
      !action ||
      // In Proton, only extension actions provide a context menu.
      (UrlbarPrefs.get("browser.proton.urlbar.enabled") && !action.extensionID)
    ) {
      this._contextAction = null;
      event.preventDefault();
      return;
    }
    this._contextAction = action;

    let state;
    if (this._contextAction._isMozillaAction) {
      state = this._contextAction.pinnedToUrlbar
        ? "builtInPinned"
        : "builtInUnpinned";
    } else {
      state = this._contextAction.pinnedToUrlbar
        ? "extensionPinned"
        : "extensionUnpinned";
    }
    popup.setAttribute("state", state);

    let removeExtension = popup.querySelector(".removeExtensionItem");
    let { extensionID } = this._contextAction;
    let addon = extensionID && (await AddonManager.getAddonByID(extensionID));
    removeExtension.hidden = !addon;
    if (addon) {
      removeExtension.disabled = !(
        addon.permissions & AddonManager.PERM_CAN_UNINSTALL
      );
    }
  },

  /**
   * Call this from the menu item in the context menu that toggles pinning.
   */
  togglePinningForContextAction() {
    if (!this._contextAction) {
      return;
    }
    let action = this._contextAction;
    this._contextAction = null;

    action.pinnedToUrlbar = !action.pinnedToUrlbar;
    BrowserUsageTelemetry.recordWidgetChange(
      action.id,
      action.pinnedToUrlbar ? "page-action-buttons" : null,
      "pageaction-context"
    );
  },

  /**
   * Call this from the menu item in the context menu that opens about:addons.
   */
  openAboutAddonsForContextAction() {
    if (!this._contextAction) {
      return;
    }
    let action = this._contextAction;
    this._contextAction = null;

    AMTelemetry.recordActionEvent({
      object: "pageAction",
      action: "manage",
      extra: { addonId: action.extensionID },
    });

    let viewID = "addons://detail/" + encodeURIComponent(action.extensionID);
    window.BrowserOpenAddonsMgr(viewID);
  },

  /**
   * Call this from the menu item in the context menu that removes an add-on.
   */
  removeExtensionForContextAction() {
    if (!this._contextAction) {
      return;
    }
    let action = this._contextAction;
    this._contextAction = null;

    BrowserAddonUI.removeAddon(action.extensionID, "pageAction");
  },

  _contextAction: null,

  /**
   * We use this to set an attribute on the DOM node. If the attribute exists,
   * then we get the panel node's attribute and set it on the DOM node. Otherwise,
   * we get the title string and update the attribute with that value. The point is to map
   * attributes on the node to strings on the main panel.  Use this for DOM
   * nodes that don't correspond to actions, like buttons in subviews.
   *
   * @param  node (DOM node, required)
   *         The node you're setting up.
   * @param  attrName (string, required)
   *         The name of the attribute *on the node you're setting up*.
   */
  takeNodeAttributeFromPanel(node, attrName) {
    let panelAttrName = node.getAttribute(attrName);
    if (!panelAttrName && attrName == "title") {
      attrName = "label";
      panelAttrName = node.getAttribute(attrName);
    }
    if (panelAttrName) {
      let attrValue = this.panelNode.getAttribute(panelAttrName);
      if (attrValue) {
        node.setAttribute(attrName, attrValue);
      }
    }
  },

  /**
   * Call this on tab switch or when the current <browser>'s location changes.
   */
  onLocationChange() {
    for (let action of PageActions.actions) {
      action.onLocationChange(window);
    }
  },
};

/**
 * Shows the feedback popup for an action.
 *
 * @param  action (PageActions.Action, required)
 *         The action associated with the feedback.
 * @param  event (DOM event, optional)
 *         The event that triggered the feedback.
 * @param  messageId (string, optional)
 *         Can be used to set a message id that is different from the action id.
 */
function showBrowserPageActionFeedback(action, event = null, messageId = null) {
  let anchor = BrowserPageActions.panelAnchorNodeForAction(action, event);

  ConfirmationHint.show(anchor, messageId || action.id, {
    event,
    hideArrow: true,
  });
}

// built-in actions below //////////////////////////////////////////////////////

// bookmark
BrowserPageActions.bookmark = {
  onShowingInPanel(buttonNode) {
    if (buttonNode.label == "null") {
      BookmarkingUI.updateBookmarkPageMenuItem();
    }
  },

  onCommand(event, buttonNode) {
    PanelMultiView.hidePopup(BrowserPageActions.panelNode);
    BookmarkingUI.onStarCommand(event);
  },
};

// pin tab
BrowserPageActions.pinTab = {
  updateState() {
    let action = PageActions.actionForID("pinTab");
    if (!action) {
      // This action doesn't exist in Proton.
      return;
    }
    let { pinned } = gBrowser.selectedTab;
    let fluentID;
    if (pinned) {
      fluentID = "page-action-unpin-tab";
    } else {
      fluentID = "page-action-pin-tab";
    }

    let panelButton = BrowserPageActions.panelButtonNodeForActionID(action.id);
    if (panelButton) {
      document.l10n.setAttributes(panelButton, fluentID + "-panel");
      panelButton.toggleAttribute("pinned", pinned);
    }
    let urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(
      action.id
    );
    if (urlbarButton) {
      document.l10n.setAttributes(urlbarButton, fluentID + "-urlbar");
      urlbarButton.toggleAttribute("pinned", pinned);
    }
  },

  onCommand(event, buttonNode) {
    if (gBrowser.selectedTab.pinned) {
      gBrowser.unpinTab(gBrowser.selectedTab);
    } else {
      gBrowser.pinTab(gBrowser.selectedTab);
    }
  },
};

// copy URL
BrowserPageActions.copyURL = {
  onCommand(event, buttonNode) {
    PanelMultiView.hidePopup(BrowserPageActions.panelNode);
    Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper)
      .copyString(
        gURLBar.makeURIReadable(gBrowser.selectedBrowser.currentURI).displaySpec
      );
    let action = PageActions.actionForID("copyURL");
    showBrowserPageActionFeedback(action, event);
  },
};

// email link
BrowserPageActions.emailLink = {
  onCommand(event, buttonNode) {
    PanelMultiView.hidePopup(BrowserPageActions.panelNode);
    MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser);
  },
};

// send to device
BrowserPageActions.sendToDevice = {
  onBeforePlacedInWindow(browserWindow) {
    this._updateTitle();
    gBrowser.addEventListener("TabMultiSelect", event => {
      this._updateTitle();
    });
  },

  // The action's title in this window depends on the number of tabs that are
  // selected.
  _updateTitle() {
    let action = PageActions.actionForID("sendToDevice");
    let tabCount = gBrowser.selectedTabs.length;

    let panelButton = BrowserPageActions.panelButtonNodeForActionID(action.id);
    if (panelButton) {
      document.l10n.setAttributes(panelButton, action.panelFluentID, {
        tabCount,
      });
    }
    let urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(
      action.id
    );
    if (urlbarButton) {
      document.l10n.setAttributes(urlbarButton, action.urlbarFluentID, {
        tabCount,
      });
    }
  },

  onSubviewPlaced(panelViewNode) {
    let bodyNode = panelViewNode.querySelector(".panel-subview-body");
    let notReady = document.createXULElement("toolbarbutton");
    notReady.classList.add(
      "subviewbutton",
      "subviewbutton-iconic",
      "pageAction-sendToDevice-notReady"
    );
    document.l10n.setAttributes(notReady, "page-action-send-tab-not-ready");
    notReady.setAttribute("disabled", "true");
    bodyNode.appendChild(notReady);
    for (let node of bodyNode.children) {
      BrowserPageActions.takeNodeAttributeFromPanel(node, "title");
      BrowserPageActions.takeNodeAttributeFromPanel(node, "shortcut");
    }
  },

  onLocationChange() {
    let action = PageActions.actionForID("sendToDevice");
    if (!action) {
      // This action doesn't exist in Proton.
      return;
    }
    let browser = gBrowser.selectedBrowser;
    let url = browser.currentURI;
    action.setDisabled(!BrowserUtils.isShareableURL(url), window);
  },

  onShowingSubview(panelViewNode) {
    gSync.populateSendTabToDevicesView(panelViewNode);
  },
};

// add search engine
BrowserPageActions.addSearchEngine = {
  get action() {
    return PageActions.actionForID("addSearchEngine");
  },

  get engines() {
    return gBrowser.selectedBrowser.engines || [];
  },

  get strings() {
    delete this.strings;
    let uri = "chrome://browser/locale/search.properties";
    return (this.strings = Services.strings.createBundle(uri));
  },

  updateEngines() {
    if (!this.action) {
      // This action doesn't exist in Proton.
      return;
    }
    // As a slight optimization, if the action isn't in the urlbar, don't do
    // anything here except disable it.  The action's panel nodes are updated
    // when the panel is shown.
    this.action.setDisabled(!this.engines.length, window);
    if (this.action.shouldShowInUrlbar(window)) {
      this._updateTitleAndIcon();
    }
  },

  _updateTitleAndIcon() {
    if (!this.engines.length) {
      return;
    }
    let title = this.strings.GetStringFromName("searchAddFoundEngine2");
    this.action.setTitle(title, window);
    this.action.setIconURL(this.engines[0].icon, window);
  },

  onShowingInPanel() {
    this._updateTitleAndIcon();
    this.action.setWantsSubview(this.engines.length > 1, window);
    let button = BrowserPageActions.panelButtonNodeForActionID(this.action.id);
    button.setAttribute("image", this.engines[0].icon);
    button.setAttribute("uri", this.engines[0].uri);
    button.setAttribute("crop", "center");
  },

  onSubviewShowing(panelViewNode) {
    let body = panelViewNode.querySelector(".panel-subview-body");
    while (body.firstChild) {
      body.firstChild.remove();
    }
    for (let engine of this.engines) {
      let button = document.createXULElement("toolbarbutton");
      button.classList.add("subviewbutton", "subviewbutton-iconic");
      button.setAttribute("label", engine.title);
      button.setAttribute("image", engine.icon);
      button.setAttribute("uri", engine.uri);
      button.addEventListener("command", event => {
        let panelNode = panelViewNode.closest("panel");
        PanelMultiView.hidePopup(panelNode);
        this._installEngine(
          button.getAttribute("uri"),
          button.getAttribute("image")
        );
      });
      body.appendChild(button);
    }
  },

  onCommand(event, buttonNode) {
    if (!buttonNode.closest("panel")) {
      // The urlbar button was clicked.  It should have a subview if there are
      // many engines.
      let manyEngines = this.engines.length > 1;
      this.action.setWantsSubview(manyEngines, window);
      if (manyEngines) {
        return;
      }
    }
    // Either the panel button or urlbar button was clicked -- not a button in
    // the subview -- but in either case, there's only one search engine.
    // (Because this method isn't called when the panel button is clicked and it
    // shows a subview, and the many-engines case for the urlbar returned early
    // above.)
    let engine = this.engines[0];
    this._installEngine(engine.uri, engine.icon);
  },

  _installEngine(uri, image) {
    SearchUIUtils.addOpenSearchEngine(
      uri,
      image,
      gBrowser.selectedBrowser.browsingContext
    )
      .then(result => {
        if (result) {
          showBrowserPageActionFeedback(this.action);
        }
      })
      .catch(console.error);
  },
};

// share URL
BrowserPageActions.shareURL = {
  onCommand(event, buttonNode) {
    let browser = gBrowser.selectedBrowser;
    let currentURI = gURLBar.makeURIReadable(browser.currentURI).displaySpec;
    this._windowsUIUtils.shareUrl(currentURI, browser.contentTitle);
  },

  onShowingInPanel(buttonNode) {
    this._cached = false;
  },

  onShowingSubview(panelViewNode) {
    let bodyNode = panelViewNode.querySelector(".panel-subview-body");

    // We cache the providers + the UI if the user selects the share
    // panel multiple times while the panel is open.
    if (this._cached && bodyNode.children.length) {
      return;
    }

    let sharingService = this._sharingService;
    let url = gBrowser.selectedBrowser.currentURI;
    let currentURI = gURLBar.makeURIReadable(url).displaySpec;
    let shareProviders = sharingService.getSharingProviders(currentURI);
    let fragment = document.createDocumentFragment();

    let onCommand = event => {
      let shareName = event.target.getAttribute("share-name");
      if (shareName) {
        sharingService.shareUrl(
          shareName,
          currentURI,
          gBrowser.selectedBrowser.contentTitle
        );
      } else if (event.target.classList.contains("share-more-button")) {
        sharingService.openSharingPreferences();
      }
      PanelMultiView.hidePopup(BrowserPageActions.panelNode);
    };

    shareProviders.forEach(function(share) {
      let item = document.createXULElement("toolbarbutton");
      item.setAttribute("label", share.menuItemTitle);
      item.setAttribute("share-name", share.name);
      item.setAttribute("image", share.image);
      item.classList.add("subviewbutton", "subviewbutton-iconic");
      item.addEventListener("command", onCommand);
      fragment.appendChild(item);
    });

    let item = document.createXULElement("toolbarbutton");
    document.l10n.setAttributes(item, "page-action-share-more-panel");
    item.classList.add(
      "subviewbutton",
      "subviewbutton-iconic",
      "share-more-button"
    );
    item.addEventListener("command", onCommand);
    fragment.appendChild(item);

    while (bodyNode.firstChild) {
      bodyNode.firstChild.remove();
    }
    bodyNode.appendChild(fragment);
    this._cached = true;
  },
};

// Attach sharingService here so tests can override the implementation
XPCOMUtils.defineLazyServiceGetters(BrowserPageActions.shareURL, {
  _sharingService: [
    "@mozilla.org/widget/macsharingservice;1",
    "nsIMacSharingService",
  ],
  _windowsUIUtils: ["@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils"],
});
