/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { html, when } from "chrome://global/content/vendor/lit.all.mjs";
import { navigateToLink } from "chrome://browser/content/firefoxview/helpers.mjs";

import { SidebarPage } from "./sidebar-page.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  HistoryController: "resource:///modules/HistoryController.sys.mjs",
});

const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";

export class SidebarHistory extends SidebarPage {
  static queries = {
    lists: { all: "fxview-tab-list" },
    searchTextbox: "fxview-search-textbox",
  };

  constructor() {
    super();
    this._started = false;
    // Setting maxTabsLength to -1 for no max
    this.maxTabsLength = -1;
  }

  controller = new lazy.HistoryController(this, {
    component: "sidebar",
  });

  connectedCallback() {
    super.connectedCallback();
    this.controller.updateCache();
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
    } else if (!this.controller.isHistoryEmpty) {
      return this.#historyCardsTemplate();
    }
    return this.#emptyMessageTemplate();
  }

  #historyCardsTemplate() {
    return this.controller.historyVisits.map(historyItem => {
      let dateArg = JSON.stringify({ date: historyItem.items[0].time });
      return html`<moz-card
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
      </moz-card>`;
    });
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

  onSearchQuery(e) {
    this.controller.onSearchQuery(e);
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
      ${this.stylesheet()}
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
}

customElements.define("sidebar-history", SidebarHistory);
