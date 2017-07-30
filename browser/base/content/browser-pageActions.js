/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BrowserPageActions = {
  /**
   * The main page action button in the urlbar (DOM node)
   */
  get mainButtonNode() {
    delete this.mainButtonNode;
    return this.mainButtonNode = document.getElementById("pageActionButton");
  },

  /**
   * The main page action panel DOM node (DOM node)
   */
  get panelNode() {
    delete this.panelNode;
    return this.panelNode = document.getElementById("pageActionPanel");
  },

  /**
   * The photonmultiview node in the main page action panel (DOM node)
   */
  get multiViewNode() {
    delete this.multiViewNode;
    return this.multiViewNode = document.getElementById("pageActionPanelMultiView");
  },

  /**
   * The main panelview node in the main page action panel (DOM node)
   */
  get mainViewNode() {
    delete this.mainViewNode;
    return this.mainViewNode = document.getElementById("pageActionPanelMainView");
  },

  /**
   * The vbox body node in the main panelview node (DOM node)
   */
  get mainViewBodyNode() {
    delete this.mainViewBodyNode;
    return this.mainViewBodyNode = this.mainViewNode.querySelector(".panel-subview-body");
  },

  /**
   * Inits.  Call to init.
   */
  init() {
    for (let action of PageActions.actions) {
      this.placeAction(action, PageActions.insertBeforeActionIDInUrlbar(action));
    }
  },

  /**
   * Adds or removes as necessary DOM nodes for the given action.
   *
   * @param  action (PageActions.Action, required)
   *         The action to place.
   * @param  panelInsertBeforeID (string, required)
   *         The ID of the action in the panel before which the given action
   *         action should be inserted.
   * @param  urlbarInsertBeforeID (string, required)
   *         If the action is shown in the urlbar, then this is ID of the action
   *         in the urlbar before which the given action should be inserted.
   */
  placeAction(action, panelInsertBeforeID, urlbarInsertBeforeID) {
    if (action.__isSeparator) {
      this._appendPanelSeparator(action);
      return;
    }
    this.placeActionInPanel(action, panelInsertBeforeID);
    this.placeActionInUrlbar(action, urlbarInsertBeforeID);
  },

  /**
   * Adds or removes as necessary DOM nodes for the action in the panel.
   *
   * @param  action (PageActions.Action, required)
   *         The action to place.
   * @param  insertBeforeID (string, required)
   *         The ID of the action in the panel before which the given action
   *         action should be inserted.
   */
  placeActionInPanel(action, insertBeforeID) {
    let id = this._panelButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);
    if (!node) {
      let panelViewNode;
      [node, panelViewNode] = this._makePanelButtonNodeForAction(action);
      node.id = id;
      let insertBeforeNode = null;
      if (insertBeforeID) {
        let insertBeforeNodeID =
          this._panelButtonNodeIDForActionID(insertBeforeID);
        insertBeforeNode = document.getElementById(insertBeforeNodeID);
      }
      this.mainViewBodyNode.insertBefore(node, insertBeforeNode);
      action.onPlacedInPanel(node);
      if (panelViewNode) {
        action.subview.onPlaced(panelViewNode);
      }
    }
    return node;
  },

  _makePanelButtonNodeForAction(action) {
    let buttonNode = document.createElement("toolbarbutton");
    buttonNode.classList.add(
      "subviewbutton",
      "subviewbutton-iconic",
      "pageAction-panel-button"
    );
    buttonNode.setAttribute("label", action.title);
    if (action.iconURL) {
      buttonNode.style.listStyleImage = `url('${action.iconURL}')`;
    }
    if (action.nodeAttributes) {
      for (let name in action.nodeAttributes) {
        buttonNode.setAttribute(name, action.nodeAttributes[name]);
      }
    }
    let panelViewNode = null;
    if (action.subview) {
      buttonNode.classList.add("subviewbutton-nav");
      panelViewNode = this._makePanelViewNodeForAction(action, false);
      this.multiViewNode.appendChild(panelViewNode);
    }
    buttonNode.addEventListener("command", event => {
      if (panelViewNode) {
        action.subview.onShowing(panelViewNode);
        this.multiViewNode.showSubView(panelViewNode, buttonNode);
        return;
      }
      if (action.wantsIframe) {
        this._toggleTempPanelForAction(action);
        return;
      }
      this.panelNode.hidePopup();
      action.onCommand(event, buttonNode);
    });
    return [buttonNode, panelViewNode];
  },

  _makePanelViewNodeForAction(action, forUrlbar) {
    let panelViewNode = document.createElement("panelview");
    let placementID = forUrlbar ? "urlbar" : "panel";
    panelViewNode.id = `pageAction-${placementID}-${action.id}-subview`;
    panelViewNode.classList.add("PanelUI-subView");
    let bodyNode = document.createElement("vbox");
    bodyNode.id = panelViewNode.id + "-body";
    bodyNode.classList.add("panel-subview-body");
    panelViewNode.appendChild(bodyNode);
    for (let button of action.subview.buttons) {
      let buttonNode = document.createElement("toolbarbutton");
      let buttonNodeID =
        forUrlbar ? this._urlbarButtonNodeIDForActionID(action.id) :
        this._panelButtonNodeIDForActionID(action.id);
      buttonNodeID += "-" + button.id;
      buttonNode.id = buttonNodeID;
      buttonNode.classList.add("subviewbutton", "subviewbutton-iconic");
      buttonNode.setAttribute("label", button.title);
      if (button.shortcut) {
        buttonNode.setAttribute("shortcut", button.shortcut);
      }
      if (button.disabled) {
        buttonNode.setAttribute("disabled", "true");
      }
      buttonNode.addEventListener("command", event => {
        button.onCommand(event, buttonNode);
      });
      bodyNode.appendChild(buttonNode);
    }
    return panelViewNode;
  },

  _toggleTempPanelForAction(action) {
    let panelNodeID = "pageActionTempPanel";
    let panelNode = document.getElementById(panelNodeID);
    if (panelNode) {
      panelNode.hidePopup();
      return;
    }

    panelNode = document.createElement("panel");
    panelNode.id = panelNodeID;
    panelNode.classList.add("cui-widget-panel");
    panelNode.setAttribute("role", "group");
    panelNode.setAttribute("type", "arrow");
    panelNode.setAttribute("flip", "slide");
    panelNode.setAttribute("noautofocus", "true");

    let panelViewNode = null;
    let iframeNode = null;

    if (action.subview) {
      let multiViewNode = document.createElement("photonpanelmultiview");
      panelViewNode = this._makePanelViewNodeForAction(action, true);
      multiViewNode.appendChild(panelViewNode);
      panelNode.appendChild(multiViewNode);
    } else if (action.wantsIframe) {
      iframeNode = document.createElement("iframe");
      iframeNode.setAttribute("type", "content");
      panelNode.appendChild(iframeNode);
    }

    let popupSet = document.getElementById("mainPopupSet");
    popupSet.appendChild(panelNode);
    panelNode.addEventListener("popuphidden", () => {
      panelNode.remove();
    }, { once: true });

    if (panelViewNode) {
      action.subview.onPlaced(panelViewNode);
      action.subview.onShowing(panelViewNode);
    }

    this.panelNode.hidePopup();

    let urlbarNodeID = this._urlbarButtonNodeIDForActionID(action.id);
    let anchorNode =
      document.getElementById(urlbarNodeID) || this.mainButtonNode;
    panelNode.openPopup(anchorNode, "bottomcenter topright");

    if (iframeNode) {
      action.onIframeShown(iframeNode, panelNode);
    }
  },

  /**
   * Adds or removes as necessary a DOM node for the given action in the urlbar.
   *
   * @param  action (PageActions.Action, required)
   *         The action to place.
   * @param  insertBeforeID (string, required)
   *         If the action is shown in the urlbar, then this is ID of the action
   *         in the urlbar before which the given action should be inserted.
   */
  placeActionInUrlbar(action, insertBeforeID) {
    let id = this._urlbarButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);

    if (!action.shownInUrlbar) {
      if (node) {
        if (action.__urlbarNodeInMarkup) {
          node.hidden = true;
        } else {
          node.remove();
        }
      }
      return null;
    }

    let newlyPlaced = false;
    if (action.__urlbarNodeInMarkup) {
      newlyPlaced = node && node.hidden;
      node.hidden = false;
    } else if (!node) {
      newlyPlaced = true;
      node = this._makeUrlbarButtonNode(action);
      node.id = id;
    }

    if (newlyPlaced) {
      let parentNode = this.mainButtonNode.parentNode;
      let insertBeforeNode = null;
      if (insertBeforeID) {
        let insertBeforeNodeID =
          this._urlbarButtonNodeIDForActionID(insertBeforeID);
        insertBeforeNode = document.getElementById(insertBeforeNodeID);
      }
      parentNode.insertBefore(node, insertBeforeNode);
      action.onPlacedInUrlbar(node);

      // urlbar buttons should always have tooltips, so if the node doesn't have
      // one, then as a last resort use the label of the corresponding panel
      // button.  Why not set tooltiptext to action.title when the node is
      // created?  Because the consumer may set a title dynamically.
      if (!node.hasAttribute("tooltiptext")) {
        let panelNodeID = this._panelButtonNodeIDForActionID(action.id);
        let panelNode = document.getElementById(panelNodeID);
        if (panelNode) {
          node.setAttribute("tooltiptext", panelNode.getAttribute("label"));
        }
      }
    }

    return node;
  },

  _makeUrlbarButtonNode(action) {
    let buttonNode = document.createElement("image");
    buttonNode.classList.add("urlbar-icon");
    if (action.tooltip) {
      buttonNode.setAttribute("tooltiptext", action.tooltip);
    }
    if (action.iconURL) {
      buttonNode.style.listStyleImage = `url('${action.iconURL}')`;
    }
    if (action.nodeAttributes) {
      for (let name in action.nodeAttributes) {
        buttonNode.setAttribute(name, action.nodeAttributes[name]);
      }
    }
    buttonNode.addEventListener("click", event => {
      if (event.button != 0) {
        return;
      }
      if (action.subview || action.wantsIframe) {
        this._toggleTempPanelForAction(action);
        return;
      }
      action.onCommand(event, buttonNode);
    });
    return buttonNode;
  },

  _appendPanelSeparator(action) {
    let node = document.createElement("toolbarseparator");
    node.id = this._panelButtonNodeIDForActionID(action.id);
    this.mainViewBodyNode.appendChild(node);
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
  },

  _removeActionFromPanel(action) {
    let id = this._panelButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);
    if (node) {
      node.remove();
    }
    if (action.subview) {
      let panelViewNodeID = this._panelViewNodeIDFromActionID(action.id);
      let panelViewNode = document.getElementById(panelViewNodeID);
      if (panelViewNode) {
        panelViewNode.remove();
      }
    }
  },

  _removeActionFromUrlbar(action) {
    let id = this._urlbarButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);
    if (node) {
      node.remove();
    }
  },

  /**
   * Updates the DOM nodes of an action to reflect its changed iconURL.
   *
   * @param  action (PageActions.Action, required)
   *         The action to update.
   */
  updateActionIconURL(action) {
    let url = action.iconURL ? `url('${action.iconURL}')` : null;
    let nodeIDs = [
      this._panelButtonNodeIDForActionID(action.id),
      this._urlbarButtonNodeIDForActionID(action.id),
    ];
    for (let nodeID of nodeIDs) {
      let node = document.getElementById(nodeID);
      if (node) {
        if (url) {
          node.style.listStyleImage = url;
        } else {
          node.style.removeProperty("list-style-image");
        }
      }
    }
  },

  /**
   * Updates the DOM nodes of an action to reflect its changed title.
   *
   * @param  action (PageActions.Action, required)
   *         The action to update.
   */
  updateActionTitle(action) {
    let id = this._panelButtonNodeIDForActionID(action.id);
    let node = document.getElementById(id);
    if (node) {
      node.setAttribute("label", action.title);
    }
  },

  /**
   * Returns the action for a node.
   *
   * @param  node (DOM node, required)
   *         A button DOM node, either one that's shown in the page action panel
   *         or the urlbar.
   * @return (PageAction.Action) The node's related action, or null if none.
   */
  actionForNode(node) {
    if (!node) {
      return null;
    }
    let actionID = this._actionIDForNodeID(node.id);
    return PageActions.actionForID(actionID);
  },

  _panelButtonNodeIDForActionID(actionID) {
    return "pageAction-panel-" + actionID;
  },

  _urlbarButtonNodeIDForActionID(actionID) {
    let action = PageActions.actionForID(actionID);
    if (action && action.urlbarIDOverride) {
      return action.urlbarIDOverride;
    }
    return "pageAction-urlbar-" + actionID;
  },

  _actionIDForNodeID(nodeID) {
    if (!nodeID) {
      return null;
    }
    let match = nodeID.match(/^pageAction-(?:panel|urlbar)-(.+)$/);
    return match ? match[1] : null;
  },

  /**
   * Call this when the main page action button in the urlbar is activated.
   *
   * @param  event (DOM event, required)
   *         The click or whatever event.
   */
  mainButtonClicked(event) {
    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN)) {
      return;
    }

    for (let action of PageActions.actions) {
      let buttonNodeID = this._panelButtonNodeIDForActionID(action.id);
      let buttonNode = document.getElementById(buttonNodeID);
      action.onShowingInPanel(buttonNode);
    }

    this.panelNode.hidden = false;
    this.panelNode.openPopup(this.mainButtonNode, {
      position: "bottomcenter topright",
      triggerEvent: event,
    });
  },

  /**
   * Call this on the contextmenu event.  Note that this is called before
   * onContextMenuShowing.
   *
   * @param  event (DOM event, required)
   *         The contextmenu event.
   */
  onContextMenu(event) {
    let node = event.originalTarget;
    this._contextAction = this.actionForNode(node);
  },

  /**
   * Call this on the context menu's popupshowing event.
   *
   * @param  event (DOM event, required)
   *         The popupshowing event.
   * @param  popup (DOM node, required)
   *         The context menu popup DOM node.
   */
  onContextMenuShowing(event, popup) {
    if (event.target != popup) {
      return;
    }
    // Right now there's only one item in the context menu, to toggle the
    // context action's shown-in-urlbar state.  Update it now.
    let toggleItem = popup.firstChild;
    let toggleItemLabel = null;
    if (this._contextAction) {
      toggleItem.disabled = false;
      if (this._contextAction.shownInUrlbar) {
        toggleItemLabel = toggleItem.getAttribute("remove-label");
      }
    }
    if (!toggleItemLabel) {
      toggleItemLabel = toggleItem.getAttribute("add-label");
    }
    toggleItem.label = toggleItemLabel;
  },

  /**
   * Call this from the context menu's toggle menu item.
   */
  toggleShownInUrlbarForContextAction() {
    if (!this._contextAction) {
      return;
    }
    this._contextAction.shownInUrlbar = !this._contextAction.shownInUrlbar;
  },

  _contextAction: null,

  /**
   * A bunch of strings (labels for actions and the like) are defined in DTD,
   * but actions are created in JS.  So what we do is add a bunch of attributes
   * to the page action panel's definition in the markup, whose values hold
   * these DTD strings.  Then when each built-in action is set up, we get the
   * related strings from the panel node and set up the action's node with them.
   *
   * The convention is to set for example the "title" property in an action's JS
   * definition to the name of the attribute on the panel node that holds the
   * actual title string.  Then call this function, passing the action's related
   * DOM node and the name of the attribute that you are setting on the DOM
   * node -- "label" or "title" in this example (either will do).
   *
   * @param  node (DOM node, required)
   *         The node of an action you're setting up.
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
};


// built-in actions below //////////////////////////////////////////////////////

// bookmark
BrowserPageActions.bookmark = {
  onShowingInPanel(buttonNode) {
    // Update the button label via the bookmark observer.
    BookmarkingUI.updateBookmarkPageMenuItem();
  },

  onCommand(event, buttonNode) {
    BrowserPageActions.panelNode.hidePopup();
    BookmarkingUI.onStarCommand(event);
  },
};

// copy URL
BrowserPageActions.copyURL = {
  onPlacedInPanel(buttonNode) {
    BrowserPageActions.takeNodeAttributeFromPanel(buttonNode, "title");
  },

  onCommand(event, buttonNode) {
    BrowserPageActions.panelNode.hidePopup();
    Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper)
      .copyString(gBrowser.selectedBrowser.currentURI.spec);
  },
};

// email link
BrowserPageActions.emailLink = {
  onPlacedInPanel(buttonNode) {
    BrowserPageActions.takeNodeAttributeFromPanel(buttonNode, "title");
  },

  onCommand(event, buttonNode) {
    BrowserPageActions.panelNode.hidePopup();
    MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser);
  },
};

// send to device
BrowserPageActions.sendToDevice = {
  onPlacedInPanel(buttonNode) {
    BrowserPageActions.takeNodeAttributeFromPanel(buttonNode, "title");
  },

  onSubviewPlaced(panelViewNode) {
    let bodyNode = panelViewNode.firstChild;
    for (let node of bodyNode.childNodes) {
      BrowserPageActions.takeNodeAttributeFromPanel(node, "title");
      BrowserPageActions.takeNodeAttributeFromPanel(node, "shortcut");
    }
  },

  onShowingInPanel(buttonNode) {
    let browser = gBrowser.selectedBrowser;
    let url = browser.currentURI.spec;
    if (gSync.isSendableURI(url)) {
      buttonNode.removeAttribute("disabled");
    } else {
      buttonNode.setAttribute("disabled", "true");
    }
  },

  onShowingSubview(panelViewNode) {
    let browser = gBrowser.selectedBrowser;
    let url = browser.currentURI.spec;
    let title = browser.contentTitle;

    let bodyNode = panelViewNode.firstChild;

    // This is on top because it also clears the device list between state
    // changes.
    gSync.populateSendTabToDevicesMenu(bodyNode, url, title, (clientId, name, clientType) => {
      if (!name) {
        return document.createElement("toolbarseparator");
      }
      let item = document.createElement("toolbarbutton");
      item.classList.add("pageAction-sendToDevice-device", "subviewbutton");
      if (clientId) {
        item.classList.add("subviewbutton-iconic");
      }
      item.setAttribute("tooltiptext", name);
      return item;
    });

    bodyNode.removeAttribute("state");
    // In the first ~10 sec after startup, Sync may not be loaded and the list
    // of devices will be empty.
    if (gSync.syncConfiguredAndLoading) {
      bodyNode.setAttribute("state", "notready");
      // Force a background Sync
      Services.tm.dispatchToMainThread(async () => {
        await Weave.Service.sync([]);  // [] = clients engine only
        // There's no way Sync is still syncing at this point, but we check
        // anyway to avoid infinite looping.
        if (!window.closed && !gSync.syncConfiguredAndLoading) {
          this.onShowingSubview(panelViewNode);
        }
      });
    }
  },
};
