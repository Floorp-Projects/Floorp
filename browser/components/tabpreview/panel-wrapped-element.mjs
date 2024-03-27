/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * MozPanelWrappedElement bridges XULPanel's imperative API with a more
 * Lit-friendly declarative option. Lit components whose display and positioning
 * should be handled by a XUL panel can extend this class to control this behavior
 * declaratively.
 *
 * The panel can be shown or hidden by changing this.panelState, and repositioned
 * by changing this.panelAnchor.
 *
 * NOTE: This likely won't work correctly unless the class that inherits
 * MozPanelWrappedElement is at the top of the Lit component stack, hence the need
 * to extend this class instead of just wrapping the component tree with something.
 *
 * @property {string} panelID ID attribute to use for the panel element
 * @property {"open"|"closed"} panelState controls whether panel is showing
 * @property {HTMLElement} panelAnchor element that the panel should anchor to
 * @property {boolean} noautofocus sets the panel's noautofocus attribute
 * @property {boolean} norolluponanchor sets the panel's norolluponanchor attribute
 * @property {boolean} rolluponmousewheel sets the panel's rolluponmousewheel attribute
 * @property {boolean} consumeoutsideclicks sets the panel's consumeoutsideclicks attribute
 * @property {boolean} level sets the panel's level attribute
 */
export class MozPanelWrappedElement extends MozLitElement {
  static properties = {
    panelID: { type: String },
    panelState: { type: String },
    panelAnchor: { type: HTMLElement },
    noautofocus: { type: Boolean },
    norolluponanchor: { type: Boolean },
    rolluponmousewheel: { type: Boolean },
    consumeoutsideclicks: { type: String },
    level: { type: String },
  };

  /**
   * Override this in a child class to do something when the popup is shown
   */
  on_popupshown() {}

  /**
   * Override this in a child class to do something when the popup is hidden
   */
  on_popuphidden() {}

  createRenderRoot() {
    if (!document.createXULElement) {
      console.error(
        "Unable to create panel: document.createXULElement is not available"
      );
      return super.createRenderRoot();
    }
    this.attachShadow({ mode: "open" });
    this._panel = document.createXULElement("panel");
    this._panel.setAttribute("id", "tabpreviewpanel");
    this.syncPanelAttributes();
    this._panel.addEventListener("popupshown", this.on_popupshown.bind(this));
    this._panel.addEventListener("popuphidden", this.on_popuphidden.bind(this));
    document.body.appendChild(this._panel);
    this.shadowRoot.append(this._panel);
    return this._panel;
  }

  syncPanelAttributes() {
    this._panel.setAttribute("noautofocus", this.noautofocus);
    this._panel.setAttribute("norolluponanchor", this.norolluponanchor);
    this._panel.setAttribute(
      "consumeoutsideclicks",
      this.consumeoutsideclicks.toString()
    );
    this._panel.setAttribute("rolluponmousewheel", this.rolluponmousewheel);
    this._panel.setAttribute("level", this.level);
  }

  updated(changedProperties) {
    this.syncPanelAttributes();
    if (changedProperties.has("panelState")) {
      if (
        this.panelState === "open" &&
        !["open", "showing"].includes(this._panel.state)
      ) {
        this._panel.openPopup(this.panelAnchor, {
          position: "bottomleft topleft",
          y: -2,
          isContextMenu: false,
        });
      } else if (
        this.panelState === "closed" &&
        !["closed", "hiding"].includes(this._panel.state)
      ) {
        this._panel.hidePopup();
      }
    } else if (
      this.panelState === "open" &&
      changedProperties.has("panelAnchor")
    ) {
      this._panel.moveToAnchor(this.panelAnchor, "bottomleft topleft", 0, -2);
    }
  }
}
