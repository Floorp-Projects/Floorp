/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html, when } from "chrome://global/content/vendor/lit.all.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-search-textbox.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-tab-list.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-card.mjs";
import { HistoryController } from "chrome://browser/content/firefoxview/HistoryController.mjs";
import { navigateToLink } from "chrome://browser/content/firefoxview/helpers.mjs";

const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";

export class SidebarHistory extends MozLitElement {
  constructor() {
    super();
    this._started = false;
    // Setting maxTabsLength to -1 for no max
    this.maxTabsLength = -1;
  }

  controller = new HistoryController(this, {
    component: "sidebar",
  });

  connectedCallback() {
    super.connectedCallback();
    this.controller.updateAllHistoryItems();
  }

  onPrimaryAction(e) {
    navigateToLink(e);
  }

  deleteFromHistory() {
    this.controller.deleteFromHistory();
  }

  /**
   * The template to use for cards-container.
   */
  get cardsTemplate() {
    if (this.controller.searchResults) {
      return this.#searchResultsTemplate();
    } else if (this.controller.allHistoryItems.size) {
      return this.#historyCardsTemplate();
    }
    return this.#emptyMessageTemplate();
  }

  #historyCardsTemplate() {
    let cardsTemplate = [];
    this.controller.historyMapByDate.forEach(historyItem => {
      if (historyItem.items.length) {
        let dateArg = JSON.stringify({ date: historyItem.items[0].time });
        cardsTemplate.push(html`<moz-card
          type="accordion"
          data-l10n-attrs="heading"
          data-l10n-id=${historyItem.l10nId}
          data-l10n-args=${dateArg}
        >
          <div>
            <fxview-tab-list
              compactRows
              class="with-context-menu"
              maxTabsLength=${this.maxTabsLength}
              .tabItems=${this.getTabItems(historyItem.items)}
              @fxview-tab-list-primary-action=${this.onPrimaryAction}
              .updatesPaused=${false}
            >
            </fxview-tab-list>
          </div>
        </moz-card>`);
      }
    });
    return cardsTemplate;
  }

  #emptyMessageTemplate() {
    let descriptionHeader;
    let descriptionLabels;
    let descriptionLink;
    if (Services.prefs.getBoolPref(NEVER_REMEMBER_HISTORY_PREF, false)) {
      // History pref set to never remember history
      descriptionHeader = "firefoxview-dont-remember-history-empty-header";
      descriptionLabels = [
        "firefoxview-dont-remember-history-empty-description",
        "firefoxview-dont-remember-history-empty-description-two",
      ];
      descriptionLink = {
        url: "about:preferences#privacy",
        name: "history-settings-url-two",
      };
    } else {
      descriptionHeader = "firefoxview-history-empty-header";
      descriptionLabels = [
        "firefoxview-history-empty-description",
        "firefoxview-history-empty-description-two",
      ];
      descriptionLink = {
        url: "about:preferences#privacy",
        name: "history-settings-url",
      };
    }
    return html`
      <fxview-empty-state
        headerLabel=${descriptionHeader}
        .descriptionLabels=${descriptionLabels}
        .descriptionLink=${descriptionLink}
        class="empty-state history"
        ?isSelectedTab=${this.selectedTab}
        mainImageUrl="chrome://browser/content/firefoxview/history-empty.svg"
      >
      </fxview-empty-state>
    `;
  }

  #searchResultsTemplate() {
    return html` <moz-card
      data-l10n-attrs="heading"
      data-l10n-id="sidebar-search-results-header"
      data-l10n-args=${JSON.stringify({
        query: this.controller.searchQuery,
      })}
    >
      <div>
        ${when(
          this.controller.searchResults.length,
          () =>
            html`<h3
              slot="secondary-header"
              data-l10n-id="firefoxview-search-results-count"
              data-l10n-args="${JSON.stringify({
                count: this.controller.searchResults.length,
              })}"
            ></h3>`
        )}
        <fxview-tab-list
          compactRows
          maxTabsLength="-1"
          .searchQuery=${this.controller.searchQuery}
          .tabItems=${this.getTabItems(this.controller.searchResults)}
          @fxview-tab-list-primary-action=${this.onPrimaryAction}
          .updatesPaused=${false}
        >
        </fxview-tab-list>
      </div>
    </moz-card>`;
  }

  async onChangeSortOption(e) {
    await this.controller.onChangeSortOption(e);
  }

  async onSearchQuery(e) {
    await this.controller.onSearchQuery(e);
  }

  getTabItems(items) {
    return items.map(item => ({
      ...item,
      secondaryL10nId: null,
      secondaryL10nArgs: null,
    }));
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar.css"
      />
      <div class="container">
        <div class="history-sort-option">
          <div class="history-sort-option">
            <fxview-search-textbox
              data-l10n-id="firefoxview-search-text-box-history"
              data-l10n-attrs="placeholder"
              @fxview-search-textbox-query=${this.onSearchQuery}
              .size=${15}
            ></fxview-search-textbox>
          </div>
        </div>
        ${this.cardsTemplate}
      </div>
    `;
  }

  willUpdate() {
    if (this.controller.allHistoryItems.size) {
      // onChangeSortOption() will update history data once it has been fetched
      // from the API.
      this.controller.createHistoryMaps();
    }
  }
}

customElements.define("sidebar-history", SidebarHistory);
