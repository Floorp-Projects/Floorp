/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  SearchUIUtils: "resource:///modules/SearchUIUtils.sys.mjs",
});

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
      // This method may be called for the same action more than once
      // (e.g. when an extension does call pageAction.show/hidden to
      // enable or disable its own pageAction and we will have to
      // update the urlbar overflow panel accordingly).
      //
      // Ensure we don't add the same actions more than once (otherwise we will
      // not remove all the entries in _removeActionFromPanel).
      if (
        this._actionsToLazilyPlaceInPanel.findIndex(a => a.id == action.id) >= 0
      ) {
        return;
      }
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
    PanelMultiView.openPopup(panelNode, anchorNode, {
      position: "bottomright topright",
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
    let buttonNode = document.createXULElement("hbox");
    buttonNode.classList.add("urlbar-page-action");
    if (action.extensionID) {
      buttonNode.classList.add("urlbar-addon-page-action");
    }
    buttonNode.setAttribute("actionid", action.id);
    buttonNode.setAttribute("role", "button");
    let commandHandler = event => {
      this.doCommandForAction(action, event, buttonNode);
    };
    buttonNode.addEventListener("click", commandHandler);
    buttonNode.addEventListener("keypress", commandHandler);

    let imageNode = document.createXULElement("image");
    imageNode.classList.add("urlbar-icon");
    buttonNode.appendChild(imageNode);
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
    // Extension page actions should behave like a transient action,
    // and be hidden from the urlbar overflow menu if they
    // are disabled (as in the urlbar when the overflow menu isn't available)
    //
    // TODO(Bug 1704139): as a follow up we may look into just set on all
    // extension pageActions `_transient: true`, at least once we sunset
    // the proton preference and we don't need the pre-Proton behavior anymore,
    // and remove this special case.
    const isProtonExtensionAction = action.extensionID;

    if (action.__transient || isProtonExtensionAction) {
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
    if (panelNode) {
      panelNode.setAttribute("label", title);
    }
    if (urlbarNode) {
      urlbarNode.setAttribute("aria-label", title);
      // tooltiptext falls back to the title, so update it too if necessary.
      let tooltip = action.getTooltip(window);
      if (!tooltip) {
        urlbarNode.setAttribute("tooltiptext", title);
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
      // When a page action is clicked, `node` will be an ancestor of
      // a node corresponding to an action. `node` will be the page action node
      // itself when a page action is selected with the keyboard. That's because
      // the semantic meaning of page action is on an hbox that contains an
      // <image>.
      for (let n = node.parentNode; n && !action; n = n.parentNode) {
        if (n.id == "page-action-buttons" || n.localName == "panelview") {
          // We reached the page-action-buttons or panelview container.
          // Stop looking; no action was found.
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
    PanelMultiView.openPopup(this.panelNode, this.mainButtonNode, {
      position: "bottomright topright",
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
    // Only extension actions provide a context menu.
    if (!action?.extensionID) {
      this._contextAction = null;
      event.preventDefault();
      return;
    }
    this._contextAction = action;

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
   * Call this on tab switch or when the current <browser>'s location changes.
   */
  onLocationChange() {
    for (let action of PageActions.actions) {
      action.onLocationChange(window);
    }
  },
};

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
