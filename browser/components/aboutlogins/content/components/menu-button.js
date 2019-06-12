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

    this.reflectFluentStrings();

    this.shadowRoot.querySelector(".menu-button").addEventListener("click", this);
  }

  static get reflectedFluentIDs() {
    return ["button-title", "menuitem-preferences"];
  }

  static get observedAttributes() {
    return MenuButton.reflectedFluentIDs;
  }

  handleSpecialCaseFluentString(attrName) {
    if (attrName != "button-title") {
      return false;
    }

    this.shadowRoot.querySelector(".menu-button").setAttribute("title", this.getAttribute(attrName));
    return true;
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        // Skip the catch-all event listener if it was the menu-button
        // that was clicked on.
        if (event.currentTarget == document.documentElement &&
            event.target == this &&
            event.originalTarget == this.shadowRoot.querySelector(".menu-button")) {
          return;
        }
        if (event.originalTarget.classList.contains("menuitem-preferences")) {
          document.dispatchEvent(new CustomEvent("AboutLoginsOpenPreferences", {
            bubbles: true,
          }));
          this.hideMenu();
          return;
        }
        this.toggleMenu();
        break;
      }
    }
  }

  toggleMenu() {
    let wasHidden = this.shadowRoot.querySelector(".menu").getAttribute("aria-hidden") == "true";
    if (wasHidden) {
      this.showMenu();
    } else {
      this.hideMenu();
    }
  }

  hideMenu() {
    this.shadowRoot.querySelector(".menu").setAttribute("aria-hidden", "true");
    document.documentElement.removeEventListener("click", this, true);
  }

  showMenu() {
    this.shadowRoot.querySelector(".menu").setAttribute("aria-hidden", "false");

    // Add a catch-all event listener to close the menu.
    document.documentElement.addEventListener("click", this, true);
  }
}
customElements.define("menu-button", MenuButton);
