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
 * Vertical strip attached to the launcher that provides an entry point
 * to various sidebar panels.
 *
 */
export default class SidebarLauncher extends MozLitElement {
  static properties = {
    topActions: { type: Array },
    bottomActions: { type: Array },
    selectedView: { type: String },
    open: { type: Boolean },
  };

  constructor() {
    super();
    this.topActions = [
      {
        icon: `url("chrome://browser/skin/insights.svg")`,
        view: null,
        l10nId: "sidebar-launcher-insights",
      },
    ];

    this.bottomActions = [
      {
        l10nId: "sidebar-menu-history",
        icon: `url("chrome://browser/content/firefoxview/view-history.svg")`,
        view: "viewHistorySidebar",
      },
      {
        l10nId: "sidebar-menu-bookmarks",
        icon: `url("chrome://browser/skin/bookmark-hollow.svg")`,
        view: "viewBookmarksSidebar",
      },
      {
        l10nId: "sidebar-menu-synced-tabs",
        icon: `url("chrome://browser/skin/device-phone.svg")`,
        view: "viewTabsSidebar",
      },
    ];

    this.selectedView = window.SidebarUI.currentID;
    this.open = window.SidebarUI.isOpen;
    this.menuMutationObserver = new MutationObserver(() =>
      this.#setExtensionItems()
    );
  }

  connectedCallback() {
    super.connectedCallback();
    this._sidebarBox = document.getElementById("sidebar-box");
    this._sidebarBox.addEventListener("sidebar-show", this);
    this._sidebarBox.addEventListener("sidebar-hide", this);
    this._sidebarMenu = document.getElementById("viewSidebarMenu");

    this.menuMutationObserver.observe(this._sidebarMenu, {
      childList: true,
      subtree: true,
    });
    this.#setExtensionItems();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this._sidebarBox.removeEventListener("sidebar-show", this);
    this._sidebarBox.removeEventListener("sidebar-hide", this);
    this.menuMutationObserver.disconnect();
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

  #setExtensionItems() {
    for (let item of this._sidebarMenu.children) {
      if (item.id.endsWith("-sidebar-action")) {
        this.topActions.push({
          tooltiptext: item.label,
          icon: item.style.getPropertyValue("--webextension-menuitem-image"),
          view: item.id.slice("menubar_menu_".length),
        });
      }
    }
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
    }
  }

  showView(e) {
    let view = e.target.getAttribute("view");
    window.SidebarUI.toggle(view);
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
    >
    </moz-button>`;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar-launcher.css"
      />
      <div class="wrapper">
        <div class="top-actions actions-list">
          ${this.topActions.map(action => this.entrypointTemplate(action))}
        </div>
        <div class="bottom-actions actions-list">
          ${this.bottomActions.map(action => this.entrypointTemplate(action))}
        </div>
      </div>
    `;
  }
}
customElements.define("sidebar-launcher", SidebarLauncher);
