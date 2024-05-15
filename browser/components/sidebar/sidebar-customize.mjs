/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, styleMap } from "chrome://global/content/vendor/lit.all.mjs";

import { SidebarPage } from "./sidebar-page.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

const l10nMap = new Map([
  ["viewHistorySidebar", "sidebar-customize-history"],
  ["viewTabsSidebar", "sidebar-customize-synced-tabs"],
  ["viewBookmarksSidebar", "sidebar-customize-bookmarks"],
]);

export class SidebarCustomize extends SidebarPage {
  static queries = {
    toolInputs: { all: ".customize-firefox-tools input" },
  };

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener("SidebarItemChanged", this);
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener("SidebarItemChanged", this);
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

  getTools() {
    const toolsMap = new Map(
      [...this.getWindow().SidebarController.toolsAndExtensions]
        // eslint-disable-next-line no-unused-vars
        .filter(([key, val]) => !val.extensionId)
    );
    return toolsMap;
  }

  handleEvent(e) {
    switch (e.type) {
      case "SidebarItemChanged":
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
      <label for=${tool.view}
        ><span class="icon ghost-icon" style=${styleMap({
          "--tool-icon": tool.icon,
        })} role="presentation"/></span><span
          data-l10n-id=${this.getInputL10nId(tool.view)}
        ></span
      ></label>
    </div>`;
  }

  render() {
    return html`
      ${this.stylesheet()}
      <link rel="stylesheet" href="chrome://browser/content/sidebar/sidebar-customize.css"></link>
      <div class="container">
        <div class="customize-header">
          <h2 data-l10n-id="sidebar-customize-header"></h2>
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
          ${[...this.getTools().values()].map(tool => this.inputTemplate(tool))}
          </div>
        </div>
      </div>
    `;
  }
}

customElements.define("sidebar-customize", SidebarCustomize);
