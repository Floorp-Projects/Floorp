/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ReflectedFluentElement from "chrome://browser/content/aboutlogins/components/reflected-fluent-element.js";

export default class MenuButton extends ReflectedFluentElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }

    let MenuButtonTemplate = document.querySelector("#menu-button-template");
    this.attachShadow({mode: "open"})
        .appendChild(MenuButtonTemplate.content.cloneNode(true));

    for (let menuitem of this.shadowRoot.querySelectorAll(".menuitem-button[data-supported-platforms]")) {
      let supportedPlatforms = menuitem.dataset.supportedPlatforms.split(",").map(platform => platform.trim());
      if (supportedPlatforms.includes(navigator.platform)) {
        menuitem.hidden = false;
      }
    }

    this._menu = this.shadowRoot.querySelector(".menu");
    this._menuButton = this.shadowRoot.querySelector(".menu-button");

    this._menuButton.addEventListener("click", this);

    super.connectedCallback();
  }

  static get reflectedFluentIDs() {
    return [
      "button-title",
      "menuitem-import",
      "menuitem-feedback",
      "menuitem-preferences",
    ];
  }

  static get observedAttributes() {
    return MenuButton.reflectedFluentIDs;
  }

  handleSpecialCaseFluentString(attrName) {
    if (!this.shadowRoot ||
        attrName != "button-title") {
      return false;
    }

    this._menuButton.setAttribute("title", this.getAttribute(attrName));
    return true;
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        // Skip the catch-all event listener if it was the menu-button
        // that was clicked on.
        if (event.currentTarget == document.documentElement &&
            event.target == this &&
            event.originalTarget == this._menuButton) {
          return;
        }
        let classList = event.originalTarget.classList;
        if (classList.contains("menuitem-import") ||
            classList.contains("menuitem-feedback") ||
            classList.contains("menuitem-preferences")) {
          let eventName = event.originalTarget.dataset.eventName;
          document.dispatchEvent(new CustomEvent(eventName, {
            bubbles: true,
          }));
          this._hideMenu();
          break;
        }
        this._toggleMenu();
        break;
      }
    }
  }

  _hideMenu() {
    this._menu.hidden = true;
    document.documentElement.removeEventListener("click", this, true);
  }

  _showMenu() {
    this._menu.hidden = false;

    // Add a catch-all event listener to close the menu.
    document.documentElement.addEventListener("click", this, true);
  }

  /**
   * Toggles the visibility of the menu.
   */
  _toggleMenu() {
    let wasHidden = this._menu.hidden;
    if (wasHidden) {
      this._showMenu();
    } else {
      this._hideMenu();
    }
  }
}
customElements.define("menu-button", MenuButton);
