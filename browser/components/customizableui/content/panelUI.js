/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
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
      menuButton: "PanelUI-menu-button",
      panel: "PanelUI-popup",
    };
  },

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

    for (let event of this.kEvents) {
      this.panel.addEventListener(event, this);
    }
  },

  uninit: function() {
    for (let event of this.kEvents) {
      this.panel.removeEventListener(event, this);
    }
  },

  /**
   * Customize mode extracts the mainView and puts it somewhere else while the
   * user customizes. Upon completion, this function can be called to put the
   * panel back to where it belongs in normal browsing mode.
   *
   * @param aMainView
   *        The mainView node to put back into place.
   */
  replaceMainView: function(aMainView) {
    this.multiView.insertBefore(aMainView, this.multiView.firstChild);
  },

  /**
   * Opens the menu panel if it's closed, or closes it if it's
   * open. If the event target has a child with the toolbarbutton-icon
   * attribute, the panel will be anchored on that child. Otherwise, the panel
   * is anchored on the event target itself.
   *
   * @param aEvent the event that triggers the toggle.
   */
  toggle: function(aEvent) {
    if (this.panel.state == "open") {
      this.hide();
    } else if (this.panel.state == "closed") {
      this.ensureRegistered();

      let anchor = aEvent.target;
      let iconAnchor =
        document.getAnonymousElementByAttribute(anchor, "class",
                                                "toolbarbutton-icon");
      this.panel.openPopup(iconAnchor || anchor, "bottomcenter topright");
    }
  },

  /**
   * If the menu panel is being shown, hide it.
   */
  hide: function() {
    this.panel.hidePopup();
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "popupshowing":
        // Fall through
      case "popupshown":
        // Fall through
      case "popuphiding":
        // Fall through
      case "popuphidden": {
        this._updatePanelButton(aEvent.target);
        break;
      }
    }
  },

  /**
   * Registering the menu panel is done lazily for performance reasons. This
   * method is exposed so that CustomizationMode can force registration in the
   * event that customization mode is started before the panel has had a chance
   * to register itself.
   */
  ensureRegistered: function() {
    CustomizableUI.registerMenuPanel(this.contents);
  },

  /**
   * Switch the panel to the main view if it's not already
   * in that view.
   */
  showMainView: function() {
    this.multiView.showMainView();
  },

  /**
   * Shows a subview in the panel with a given ID.
   *
   * @param aViewId the ID of the subview to show.
   * @param aAnchor the element that spawned the subview.
   * @param aPlacementArea the CustomizableUI area that aAnchor is in.
   */
  showSubView: function(aViewId, aAnchor, aPlacementArea) {
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
    } else {
      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
      let evt = document.createEvent("CustomEvent");
      evt.initCustomEvent("ViewShowing", true, true, viewNode);
      viewNode.dispatchEvent(evt);

      let tempPanel = document.createElement("panel");
      tempPanel.appendChild(viewNode);
      tempPanel.setAttribute("type", "arrow");
      tempPanel.setAttribute("id", "customizationui-widget-panel");
      document.getElementById(CustomizableUI.AREA_NAVBAR).appendChild(tempPanel);
      tempPanel.addEventListener("popuphidden", function panelRemover() {
        tempPanel.removeEventListener("popuphidden", panelRemover);
        this.multiView.appendChild(viewNode);
        tempPanel.parentElement.removeChild(tempPanel);
      }.bind(this));

      tempPanel.openPopup(aAnchor, "bottomcenter topright");
    }
  },

  /**
   * Sets the anchor node into the open or closed state, depending
   * on the state of the panel.
   */
  _updatePanelButton: function() {
    this.menuButton.open = this.panel.state == "open" ||
                           this.panel.state == "showing";
  }
};
