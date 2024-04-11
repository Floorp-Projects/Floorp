/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  SyncedTabsController: "resource:///modules/SyncedTabsController.sys.mjs",
});

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import {
  escapeHtmlEntities,
  navigateToLink,
} from "chrome://browser/content/firefoxview/helpers.mjs";

import { SidebarPage } from "./sidebar-page.mjs";

class SyncedTabsInSidebar extends SidebarPage {
  controller = new lazy.SyncedTabsController(this);

  constructor() {
    super();
    this.onSearchQuery = this.onSearchQuery.bind(this);
  }

  connectedCallback() {
    super.connectedCallback();
    this.controller.addSyncObservers();
    this.controller.updateStates();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.controller.removeSyncObservers();
  }

  /**
   * The template shown when the list of synced devices is currently
   * unavailable.
   *
   * @param {object} options
   * @param {string} options.action
   * @param {string} options.buttonLabel
   * @param {string[]} options.descriptionArray
   * @param {string} options.descriptionLink
   * @param {boolean} options.error
   * @param {string} options.header
   * @param {string} options.headerIconUrl
   * @param {string} options.mainImageUrl
   * @returns {TemplateResult}
   */
  messageCardTemplate({
    action,
    buttonLabel,
    descriptionArray,
    descriptionLink,
    error,
    header,
    headerIconUrl,
    mainImageUrl,
  }) {
    return html`
      <fxview-empty-state
        headerLabel=${header}
        .descriptionLabels=${descriptionArray}
        .descriptionLink=${ifDefined(descriptionLink)}
        class="empty-state synced-tabs error"
        isSelectedTab
        mainImageUrl="${ifDefined(mainImageUrl)}"
        ?errorGrayscale=${error}
        headerIconUrl="${ifDefined(headerIconUrl)}"
        id="empty-container"
      >
        <button
          class="primary"
          slot="primary-action"
          ?hidden=${!buttonLabel}
          data-l10n-id="${ifDefined(buttonLabel)}"
          data-action="${action}"
          @click=${e => this.controller.handleEvent(e)}
          aria-details="empty-container"
        ></button>
      </fxview-empty-state>
    `;
  }

  /**
   * The template shown for a device that has tabs.
   *
   * @param {string} deviceName
   * @param {string} deviceType
   * @param {Array} tabItems
   * @returns {TemplateResult}
   */
  deviceTemplate(deviceName, deviceType, tabItems) {
    return html`<moz-card
      type="accordion"
      .heading=${deviceName}
      icon
      class=${deviceType}
    >
      <fxview-tab-list
        compactRows
        .tabItems=${ifDefined(tabItems)}
        .updatesPaused=${false}
        .searchQuery=${this.controller.searchQuery}
        @fxview-tab-list-primary-action=${navigateToLink}
      />
    </moz-card>`;
  }

  /**
   * The template shown for a device that has no tabs.
   *
   * @param {string} deviceName
   * @param {string} deviceType
   * @returns {TemplateResult}
   */
  noDeviceTabsTemplate(deviceName, deviceType) {
    return html`<moz-card
      .heading=${deviceName}
      icon
      class=${deviceType}
      data-l10n-id="firefoxview-syncedtabs-device-notabs"
    >
    </moz-card>`;
  }

  /**
   * The template shown for a device that has tabs, but no tabs that match the
   * current search query.
   *
   * @param {string} deviceName
   * @param {string} deviceType
   * @returns {TemplateResult}
   */
  noSearchResultsTemplate(deviceName, deviceType) {
    return html`<moz-card
      .heading=${deviceName}
      icon
      class=${deviceType}
      data-l10n-id="firefoxview-search-results-empty"
      data-l10n-args=${JSON.stringify({
        query: escapeHtmlEntities(this.controller.searchQuery),
      })}
    >
    </moz-card>`;
  }

  /**
   * The template shown for the list of synced devices.
   *
   * @returns {TemplateResult[]}
   */
  deviceListTemplate() {
    return Object.values(this.controller.getRenderInfo()).map(
      ({ name: deviceName, deviceType, tabItems, tabs }) => {
        if (tabItems.length) {
          return this.deviceTemplate(deviceName, deviceType, tabItems);
        } else if (tabs.length) {
          return this.noSearchResultsTemplate(deviceName, deviceType);
        }
        return this.noDeviceTabsTemplate(deviceName, deviceType);
      }
    );
  }

  render() {
    const messageCard = this.controller.getMessageCard();
    if (messageCard) {
      return [this.stylesheet(), this.messageCardTemplate(messageCard)];
    }
    return html`
      ${this.stylesheet()}
      <fxview-search-textbox
        data-l10n-id="firefoxview-search-text-box-syncedtabs"
        data-l10n-attrs="placeholder"
        @fxview-search-textbox-query=${this.onSearchQuery}
        size="15"
      ></fxview-search-textbox>
      ${this.deviceListTemplate()}
    `;
  }

  onSearchQuery(e) {
    this.controller.searchQuery = e.detail.query;
    this.requestUpdate();
  }
}

customElements.define("sidebar-syncedtabs", SyncedTabsInSidebar);
