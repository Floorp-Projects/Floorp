/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default class MenuButton extends HTMLElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }

    let MenuButtonTemplate = document.querySelector("#menu-button-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(MenuButtonTemplate.content.cloneNode(true));

    for (let menuitem of this.shadowRoot.querySelectorAll(
      ".menuitem-button[data-supported-platforms]"
    )) {
      let supportedPlatforms = menuitem.dataset.supportedPlatforms
        .split(",")
        .map(platform => platform.trim());
      if (supportedPlatforms.includes(navigator.platform)) {
        menuitem.hidden = false;
      }
    }

    this._menu = this.shadowRoot.querySelector(".menu");
    this._menuButton = this.shadowRoot.querySelector(".menu-button");

    this._menuButton.addEventListener("click", this);
    document.addEventListener("keydown", this, true);
  }

  handleEvent(event) {
    switch (event.type) {
      case "blur": {
        if (event.explicitOriginalTarget) {
          let node = event.explicitOriginalTarget;
          if (node.nodeType == Node.TEXT_NODE) {
            node = node.parentElement;
          }
          if (node.closest(".menu") == this._menu) {
            // Only hide the menu if focus has left the menu-button.
            return;
          }
        }
        this._hideMenu();
        break;
      }
      case "click": {
        // Skip the catch-all event listener if it was the menu-button
        // that was clicked on.
        if (
          event.currentTarget == document.documentElement &&
          event.target == this &&
          event.originalTarget == this._menuButton
        ) {
          return;
        }

        if (event.originalTarget == this._menuButton) {
          this._toggleMenu();
          if (!this._menu.hidden) {
            this._menuButton.focus();
          }
          return;
        }

        let classList = event.originalTarget.classList;
        if (classList.contains("menuitem-button")) {
          let eventName = event.originalTarget.dataset.eventName;
          const linkTrackingSource = "Elipsis_Menu";
          document.dispatchEvent(
            new CustomEvent(eventName, {
              bubbles: true,
              detail: linkTrackingSource,
            })
          );

          // Bug 1645365: Only hide the menu when the buttons are clicked
          // So that the menu isn't closed when non-buttons (e.g. separators, paddings) are clicked
          this._hideMenu();
        }

        // Explicitly close menu at the catch-all click event (i.e. a click outside of the menu)
        if (
          !this._menu.contains(event.originalTarget) &&
          !this._menuButton.contains(event.originalTarget)
        ) {
          this._hideMenu();
        }

        break;
      }
      case "keydown": {
        this._handleKeyDown(event);
      }
    }
  }

  _handleKeyDown(event) {
    if (event.key == "Enter" && event.originalTarget == this._menuButton) {
      event.preventDefault();
      this._toggleMenu();
      this._focusSuccessor(true);
    } else if (event.key == "Escape" && !this._menu.hidden) {
      this._hideMenu();
      this._menuButton.focus();
    } else if (
      (event.key == "ArrowDown" || event.key == "ArrowUp") &&
      !this._menu.hidden
    ) {
      event.preventDefault();
      this._focusSuccessor(event.key == "ArrowDown");
    }
  }

  _focusSuccessor(next = true) {
    let items = this._menu.querySelectorAll(".menuitem-button:not([hidden])");
    let firstItem = items[0];
    let lastItem = items[items.length - 1];

    let activeItem = this.shadowRoot.activeElement;
    let activeItemIndex = [...items].indexOf(activeItem);

    let successor = null;

    if (next) {
      if (!activeItem || activeItem === lastItem) {
        successor = firstItem;
      } else {
        successor = items[activeItemIndex + 1];
      }
    } else if (activeItem === this._menuButton || activeItem === firstItem) {
      successor = lastItem;
    } else {
      successor = items[activeItemIndex - 1];
    }

    if (this._menu.hidden) {
      this._showMenu();
    }
    if (successor.disabled) {
      if (next) {
        successor = items[activeItemIndex + 2];
      } else {
        successor = items[activeItemIndex - 2];
      }
    }
    window.AboutLoginsUtils.setFocus(successor);
  }

  _hideMenu() {
    this._menu.hidden = true;

    this.removeEventListener("blur", this);
    document.documentElement.removeEventListener("click", this, true);
  }

  _showMenu() {
    this._menu.querySelector(".menuitem-import-file").hidden = !window
      .AboutLoginsUtils.fileImportEnabled;

    this._menu.hidden = false;

    // Event listeners to close the menu
    this.addEventListener("blur", this);
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
