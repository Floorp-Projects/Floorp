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
      if (this._openMenuButton) {
        return;
      }

      const template = document.getElementById(
        "unified-extensions-item-template"
      );
      this.appendChild(template.content.cloneNode(true));

      this._actionButton = this.querySelector(
        ".unified-extensions-item-action"
      );
      this._openMenuButton = this.querySelector(
        ".unified-extensions-item-open-menu"
      );

      this._openMenuButton.addEventListener("blur", this);
      this._openMenuButton.addEventListener("focus", this);

      this.addEventListener("command", this);
      this.addEventListener("mouseout", this);
      this.addEventListener("mouseover", this);

      this.render();
    }

    handleEvent(event) {
      const { target } = event;

      switch (event.type) {
        case "command":
          if (target === this._openMenuButton) {
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
          if (target === this._openMenuButton) {
            this.removeAttribute("secondary-button-hovered");
          } else if (target === this._actionButton) {
            this._updateStateMessage();
          }
          break;

        case "focus":
        case "mouseover":
          if (target === this._openMenuButton) {
            this.setAttribute("secondary-button-hovered", true);
          } else if (target === this._actionButton) {
            this._updateStateMessage({ hover: true });
          }
          break;
      }
    }

    async _updateStateMessage({ hover = false } = {}) {
      const messages = OriginControls.getStateMessageIDs({
        policy: WebExtensionPolicy.getByID(this.addon.id),
        uri: this.ownerGlobal.gBrowser.currentURI,
      });

      if (!messages) {
        return;
      }

      const messageElement = this.querySelector(
        ".unified-extensions-item-message-default"
      );

      // We only want to adjust the height of an item in the panel when we
      // first draw it, and not on hover (even if the hover message is longer,
      // which shouldn't happen in practice but even if it was, we don't want
      // to change the height on hover).
      let adjustMinHeight = false;
      if (hover && messages.onHover) {
        this.ownerDocument.l10n.setAttributes(messageElement, messages.onHover);
      } else if (messages.default) {
        this.ownerDocument.l10n.setAttributes(messageElement, messages.default);
        adjustMinHeight = true;
      }

      await document.l10n.translateElements([messageElement]);

      if (adjustMinHeight) {
        const contentsElement = this.querySelector(
          ".unified-extensions-item-contents"
        );
        const { height } = getComputedStyle(contentsElement);
        contentsElement.style.minHeight = height;
      }
    }

    _hasAction() {
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

      this._actionButton.disabled = !this._hasAction();

      // Note that the data-extensionid attribute is used by context menu handlers
      // to identify the extension being manipulated by the context menu.
      this._openMenuButton.dataset.extensionid = this.addon.id;
      this._openMenuButton.setAttribute(
        "data-l10n-args",
        JSON.stringify({ extensionName: this.addon.name })
      );

      this._updateStateMessage();
    }
  }
);
