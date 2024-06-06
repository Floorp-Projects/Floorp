/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, when } from "chrome://global/content/vendor/lit.all.mjs";

import { SidebarPage } from "./sidebar-page.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

const l10nMap = new Map([
  ["viewHistorySidebar", "sidebar-menu-history-label"],
  ["viewTabsSidebar", "sidebar-menu-synced-tabs-label"],
  ["viewBookmarksSidebar", "sidebar-menu-bookmarks-label"],
]);

export class SidebarCustomize extends SidebarPage {
  constructor() {
    super();
    this.activeExtIndex = 0;
  }

  static properties = {
    activeExtIndex: { type: Number },
  };

  static queries = {
    toolInputs: { all: ".customize-firefox-tools input" },
    extensionLinks: { all: ".extension-link" },
  };

  connectedCallback() {
    super.connectedCallback();
    this.getWindow().addEventListener("SidebarItemAdded", this);
    this.getWindow().addEventListener("SidebarItemChanged", this);
    this.getWindow().addEventListener("SidebarItemRemoved", this);
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    this.getWindow().removeEventListener("SidebarItemAdded", this);
    this.getWindow().removeEventListener("SidebarItemChanged", this);
    this.getWindow().removeEventListener("SidebarItemRemoved", this);
  }

  get sidebarLauncher() {
    return this.getWindow().document.querySelector("sidebar-launcher");
  }

  getWindow() {
    return window.browsingContext.embedderWindowGlobal.browsingContext.window;
  }

  closeCustomizeView(e) {
    e.preventDefault();
    let view = e.target.getAttribute("view");
    this.getWindow().SidebarController.toggle(view);
  }

  handleEvent(e) {
    switch (e.type) {
      case "SidebarItemAdded":
      case "SidebarItemChanged":
      case "SidebarItemRemoved":
        this.requestUpdate();
        break;
    }
  }

  async onToggleInput(e) {
    e.preventDefault();
    this.getWindow().SidebarController.toggleTool(e.target.id);
  }

  getInputL10nId(view) {
    return l10nMap.get(view);
  }

  inputTemplate(tool) {
    return html`<div class="input-wrapper">
      <input
        type="checkbox"
        id=${tool.view}
        name=${tool.view}
        @change=${this.onToggleInput}
        ?checked=${!tool.disabled}
      />
      <label for=${tool.view}>
        <img src=${tool.iconUrl} class="icon" role="presentation" />
        <span data-l10n-id=${this.getInputL10nId(tool.view)} />
      </label>
    </div>`;
  }

  async manageAddon(extensionId) {
    await this.getWindow().BrowserAddonUI.manageAddon(
      extensionId,
      "unifiedExtensions"
    );
  }

  handleKeydown(e) {
    if (e.code == "ArrowUp") {
      if (this.activeExtIndex >= 0) {
        this.focusIndex(this.activeExtIndex - 1);
      }
    } else if (e.code == "ArrowDown") {
      if (this.activeExtIndex < this.extensionLinks.length) {
        this.focusIndex(this.activeExtIndex + 1);
      }
    } else if (
      (e.type == "keydown" && e.code == "Enter") ||
      (e.type == "keydown" && e.code == "Space")
    ) {
      this.manageAddon(e.target.getAttribute("extensionId"));
    }
  }

  focusIndex(index) {
    let extLinkList = Array.from(
      this.shadowRoot.querySelectorAll(".extension-link")
    );
    extLinkList[index].focus();
    this.activeExtIndex = index;
  }

  extensionTemplate(extension, index) {
    return html` <div class="extension-item">
      <img src=${extension.iconUrl} class="icon" role="presentation" />
      <div
        class="extension-link"
        extensionId=${extension.extensionId}
        tabindex=${index === this.activeExtIndex ? 0 : -1}
        role="list-item"
        @click=${() => this.manageAddon(extension.extensionId)}
        @keydown=${this.handleKeydown}
      >
        <a
          href="about:addons"
          tabindex="-1"
          target="_blank"
          @click=${e => e.preventDefault()}
          >${extension.tooltiptext}
        </a>
      </div>
    </div>`;
  }

  render() {
    let extensions = this.getWindow().SidebarController.getExtensions();
    return html`
      ${this.stylesheet()}
      <link rel="stylesheet" href="chrome://browser/content/sidebar/sidebar-customize.css"></link>
      <div class="container">
        <div class="customize-header">
          <h2 data-l10n-id="sidebar-menu-customize-label"></h2>
          <moz-button
            class="customize-close-button"
            @click=${this.closeCustomizeView}
            view="viewCustomizeSidebar"
            size="default"
            type="icon ghost"
          >
          </moz-button>
        </div>
        <div class="customize-firefox-tools">
          <h5 data-l10n-id="sidebar-customize-firefox-tools"></h5>
          <div class="inputs">
          ${this.getWindow()
            .SidebarController.getTools()
            .map(tool => this.inputTemplate(tool))}
          </div>
        </div>
        ${when(
          extensions.length,
          () => html`<div class="customize-extensions">
            <h5 data-l10n-id="sidebar-customize-extensions"></h5>
            <div role="list" class="extensions">
              ${extensions.map((extension, index) =>
                this.extensionTemplate(extension, index)
              )}
            </div>
          </div>`
        )}
      </div>
    `;
  }
}

customElements.define("sidebar-customize", SidebarCustomize);
