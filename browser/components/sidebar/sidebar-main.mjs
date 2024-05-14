/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  styleMap,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

/**
 * Sidebar with expanded and collapsed states that provides entry points
 * to various sidebar panels and sidebar extensions.
 */
export default class SidebarMain extends MozLitElement {
  static properties = {
    bottomActions: { type: Array },
    selectedView: { type: String },
    sidebarItems: { type: Array },
    open: { type: Boolean },
  };

  static queries = {
    extensionButtons: { all: ".tools-and-extensions > moz-button[extension]" },
    toolButtons: { all: ".tools-and-extensions > moz-button:not([extension])" },
    customizeButton: ".bottom-actions > moz-button[view=viewCustomizeSidebar]",
  };

  constructor() {
    super();
    this.bottomActions = [];
    this.selectedView = window.SidebarController.currentID;
    this.open = window.SidebarController.isOpen;
  }

  connectedCallback() {
    super.connectedCallback();
    this._sidebarBox = document.getElementById("sidebar-box");
    this._sidebarBox.addEventListener("sidebar-show", this);
    this._sidebarBox.addEventListener("sidebar-hide", this);

    window.addEventListener("SidebarItemAdded", this);
    window.addEventListener("SidebarItemChanged", this);
    window.addEventListener("SidebarItemRemoved", this);

    this.setCustomize();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this._sidebarBox.removeEventListener("sidebar-show", this);
    this._sidebarBox.removeEventListener("sidebar-hide", this);

    window.removeEventListener("SidebarItemAdded", this);
    window.removeEventListener("SidebarItemChanged", this);
    window.removeEventListener("SidebarItemRemoved", this);
  }

  getImageUrl(icon, targetURI) {
    if (window.IS_STORYBOOK) {
      return `chrome://global/skin/icons/defaultFavicon.svg`;
    }
    if (!icon) {
      if (targetURI?.startsWith("moz-extension")) {
        return "chrome://mozapps/skin/extensions/extension.svg";
      }
      return `chrome://global/skin/icons/defaultFavicon.svg`;
    }
    // If the icon is not for website (doesn't begin with http), we
    // display it directly. Otherwise we go through the page-icon
    // protocol to try to get a cached version. We don't load
    // favicons directly.
    if (icon.startsWith("http")) {
      return `page-icon:${targetURI}`;
    }
    return icon;
  }

  getToolsAndExtensions() {
    return window.SidebarController.toolsAndExtensions;
  }

  setCustomize() {
    this.bottomActions.push(
      ...window.SidebarController.getSidebarPanels([
        "viewCustomizeSidebar",
      ]).map(({ commandID, icon, revampL10nId }) => ({
        l10nId: revampL10nId,
        icon,
        view: commandID,
      }))
    );
  }

  handleEvent(e) {
    switch (e.type) {
      case "sidebar-show":
        this.selectedView = e.detail.viewId;
        this.open = true;
        break;
      case "sidebar-hide":
        this.open = false;
        break;
      case "SidebarItemAdded":
      case "SidebarItemChanged":
      case "SidebarItemRemoved":
        this.requestUpdate();
        break;
    }
  }

  showView(e) {
    let view = e.target.getAttribute("view");
    window.SidebarController.toggle(view);
  }

  buttonType(action) {
    return this.open && action.view == this.selectedView
      ? "icon"
      : "icon ghost";
  }

  entrypointTemplate(action) {
    return html`<moz-button
      class="icon-button"
      type=${this.buttonType(action)}
      view=${action.view}
      @click=${action.view ? this.showView : null}
      title=${ifDefined(action.tooltiptext)}
      data-l10n-id=${ifDefined(action.l10nId)}
      style=${styleMap({ "--action-icon": action.icon })}
      ?extension=${action.view?.includes("-sidebar-action")}
    >
    </moz-button>`;
  }

  render() {
    let toolsAndExtensions = this.getToolsAndExtensions()
      ? this.getToolsAndExtensions()
      : new Map();
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar-main.css"
      />
      <div class="wrapper">
        <div class="tools-and-extensions actions-list">
          ${[...toolsAndExtensions.values()]
            .filter(toolOrExtension => !toolOrExtension.disabled)
            .map(action => this.entrypointTemplate(action))}
        </div>
        <div class="bottom-actions actions-list">
          ${this.bottomActions.map(action => this.entrypointTemplate(action))}
        </div>
      </div>
    `;
  }
}
customElements.define("sidebar-main", SidebarMain);
