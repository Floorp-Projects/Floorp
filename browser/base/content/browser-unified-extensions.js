/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

ChromeUtils.defineESModuleGetters(this, {
  OriginControls: "resource://gre/modules/ExtensionPermissions.sys.mjs",
});

/**
 * The `unified-extensions-item` custom element is used to manage an extension
 * in the list of extensions, which is displayed when users click the unified
 * extensions (toolbar) button.
 *
 * This custom element must be initialized with `setExtension()`:
 *
 * ```
 * let item = document.createElement("unified-extensions-item");
 * item.setExtension(extension);
 * document.body.appendChild(item);
 * ```
 */
customElements.define(
  "unified-extensions-item",
  class extends HTMLElement {
    /**
     * Set the extension for this item. The item will be populated based on the
     * extension when it is rendered into the DOM.
     *
     * @param {Extension} extension The extension to use.
     */
    setExtension(extension) {
      this.extension = extension;
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
            // Anchor to the visible part of the button.
            const anchor = target.firstElementChild;
            popup.openPopup(
              anchor,
              "after_end",
              0,
              0,
              true /* isContextMenu */,
              false /* attributesOverride */,
              event
            );
          } else if (target === this._actionButton) {
            const win = event.target.ownerGlobal;
            const tab = win.gBrowser.selectedTab;

            this.extension.tabManager.addActiveTabPermission(tab);
            this.extension.tabManager.activateScripts(tab);
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
        policy: this.extension.policy,
        tab: this.ownerGlobal.gBrowser.selectedTab,
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
      const state = OriginControls.getState(
        this.extension.policy,
        this.ownerGlobal.gBrowser.selectedTab
      );

      return state && state.whenClicked && !state.hasAccess;
    }

    render() {
      if (!this.extension) {
        throw new Error(
          "unified-extensions-item requires an extension, forgot to call setExtension()?"
        );
      }

      this.setAttribute("extension-id", this.extension.id);
      this.classList.add(
        "toolbaritem-combined-buttons",
        "unified-extensions-item"
      );

      // The data-extensionid attribute is used by context menu handlers
      // to identify the extension being manipulated by the context menu.
      this._actionButton.dataset.extensionid = this.extension.id;

      const { attention } = OriginControls.getAttentionState(
        this.extension.policy,
        this.ownerGlobal
      );
      this.toggleAttribute("attention", attention);

      this.querySelector(".unified-extensions-item-name").textContent =
        this.extension.name;

      AddonManager.getAddonByID(this.extension.id).then(addon => {
        const iconURL = AddonManager.getPreferredIconURL(addon, 32, window);
        if (iconURL) {
          this.querySelector(".unified-extensions-item-icon").setAttribute(
            "src",
            iconURL
          );
        }
      });

      this._actionButton.disabled = !this.#hasAction();

      // The data-extensionid attribute is used by context menu handlers
      // to identify the extension being manipulated by the context menu.
      this._menuButton.dataset.extensionid = this.extension.id;
      this.ownerDocument.l10n.setAttributes(
        this._menuButton,
        "unified-extensions-item-open-menu",
        { extensionName: this.extension.name }
      );

      this.#setStateMessage();
    }
  }
);
