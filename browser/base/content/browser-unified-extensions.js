/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

ChromeUtils.defineModuleGetter(
  this,
  "OriginControls",
  "resource://gre/modules/ExtensionPermissions.jsm"
);

/**
 * The `unified-extensions-item` custom element is used to manage an extension
 * in the list of extensions, which is displayed when users click the unified
 * extensions (toolbar) button.
 *
 * This custom element must be initialized with `setAddon()`:
 *
 * ```
 * let item = document.createElement("unified-extensions-item");
 * item.setAddon(addon);
 * document.body.appendChild(item);
 * ```
 */
customElements.define(
  "unified-extensions-item",
  class extends HTMLElement {
    /**
     * Set the add-on for this item. The item will be populated based on the
     * add-on when it is rendered into the DOM.
     *
     * @param {AddonWrapper} addon The add-on to use.
     */
    setAddon(addon) {
      this.addon = addon;
    }

    connectedCallback() {
      if (this._menuButton) {
        return;
      }

      const template = document.getElementById(
        "unified-extensions-item-template"
      );
      this.appendChild(template.content.cloneNode(true));

      this._actionButton = this.querySelector(
        ".unified-extensions-item-action-button"
      );
      this._menuButton = this.querySelector(
        ".unified-extensions-item-menu-button"
      );
      this._messageDeck = this.querySelector(
        ".unified-extensions-item-message-deck"
      );

      // Focus/blur events are fired on specific elements only.
      this._actionButton.addEventListener("blur", this);
      this._actionButton.addEventListener("focus", this);
      this._menuButton.addEventListener("blur", this);
      this._menuButton.addEventListener("focus", this);

      this.addEventListener("command", this);
      this.addEventListener("mouseout", this);
      this.addEventListener("mouseover", this);

      this.render();
    }

    handleEvent(event) {
      const { target } = event;

      switch (event.type) {
        case "command":
          if (target === this._menuButton) {
            const popup = target.ownerDocument.getElementById(
              "unified-extensions-context-menu"
            );
            popup.openPopup(
              target,
              "after_end",
              0,
              0,
              true /* isContextMenu */,
              false /* attributesOverride */,
              event
            );
          } else if (target === this._actionButton) {
            const extension = WebExtensionPolicy.getByID(this.addon.id)
              ?.extension;
            if (!extension) {
              return;
            }

            const win = event.target.ownerGlobal;
            const tab = win.gBrowser.selectedTab;

            extension.tabManager.addActiveTabPermission(tab);
            extension.tabManager.activateScripts(tab);
          }
          break;

        case "blur":
        case "mouseout":
          this._messageDeck.selectedIndex =
            gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT;
          break;

        case "focus":
        case "mouseover":
          if (target === this._menuButton) {
            this._messageDeck.selectedIndex =
              gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER;
          } else if (target === this._actionButton) {
            this._messageDeck.selectedIndex =
              gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER;
          }
          break;
      }
    }

    #setStateMessage() {
      const messages = OriginControls.getStateMessageIDs({
        policy: WebExtensionPolicy.getByID(this.addon.id),
        uri: this.ownerGlobal.gBrowser.currentURI,
      });

      if (!messages) {
        return;
      }

      const messageDefaultElement = this.querySelector(
        ".unified-extensions-item-message-default"
      );
      this.ownerDocument.l10n.setAttributes(
        messageDefaultElement,
        messages.default
      );

      const messageHoverElement = this.querySelector(
        ".unified-extensions-item-message-hover"
      );
      this.ownerDocument.l10n.setAttributes(
        messageHoverElement,
        messages.onHover || messages.default
      );
    }

    #hasAction() {
      const policy = WebExtensionPolicy.getByID(this.addon.id);
      const state = OriginControls.getState(
        policy,
        this.ownerGlobal.gBrowser.currentURI
      );

      return state && state.whenClicked && !state.hasAccess;
    }

    render() {
      if (!this.addon) {
        throw new Error(
          "unified-extensions-item requires an add-on, forgot to call setAddon()?"
        );
      }

      this.setAttribute("extension-id", this.addon.id);
      this.classList.add("unified-extensions-item");

      // Note that the data-extensionid attribute is used by context menu handlers
      // to identify the extension being manipulated by the context menu.
      this._actionButton.dataset.extensionid = this.addon.id;

      let policy = WebExtensionPolicy.getByID(this.addon.id);
      this.toggleAttribute(
        "attention",
        OriginControls.getAttention(policy, this.ownerGlobal)
      );

      this.querySelector(
        ".unified-extensions-item-name"
      ).textContent = this.addon.name;

      const iconURL = AddonManager.getPreferredIconURL(this.addon, 32, window);
      if (iconURL) {
        this.querySelector(".unified-extensions-item-icon").setAttribute(
          "src",
          iconURL
        );
      }

      this._actionButton.disabled = !this.#hasAction();

      // Note that the data-extensionid attribute is used by context menu handlers
      // to identify the extension being manipulated by the context menu.
      this._menuButton.dataset.extensionid = this.addon.id;
      this._menuButton.setAttribute(
        "data-l10n-args",
        JSON.stringify({ extensionName: this.addon.name })
      );

      this.#setStateMessage();
    }
  }
);
