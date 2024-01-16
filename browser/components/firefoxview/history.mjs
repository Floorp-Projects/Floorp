/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { escapeHtmlEntities, isSearchEnabled } from "./helpers.mjs";
import { ViewPage } from "./viewpage.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/migration/migration-wizard.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  FirefoxViewPlacesQuery:
    "resource:///modules/firefox-view-places-query.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  ProfileAge: "resource://gre/modules/ProfileAge.sys.mjs",
});

let XPCOMUtils = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
).XPCOMUtils;

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "maxRowsPref",
  "browser.firefox-view.max-history-rows",
  -1
);

const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";
const HAS_IMPORTED_HISTORY_PREF = "browser.migrate.interactions.history";
const IMPORT_HISTORY_DISMISSED_PREF =
  "browser.tabs.firefox-view.importHistory.dismissed";

const SEARCH_RESULTS_LIMIT = 300;

class HistoryInView extends ViewPage {
  constructor() {
    super();
    this._started = false;
    this.allHistoryItems = new Map();
    this.historyMapByDate = [];
    this.historyMapBySite = [];
    // Setting maxTabsLength to -1 for no max
    this.maxTabsLength = -1;
    this.placesQuery = new lazy.FirefoxViewPlacesQuery();
    this.searchQuery = "";
    this.searchResults = null;
    this.sortOption = "date";
    this.profileAge = 8;
    this.fullyUpdated = false;
  }

  start() {
    if (this._started) {
      return;
    }
    this._started = true;

    this.#updateAllHistoryItems();
    this.placesQuery.observeHistory(data => this.#updateAllHistoryItems(data));

    this.toggleVisibilityInCardContainer();
  }

  async connectedCallback() {
    super.connectedCallback();
    await this.updateHistoryData();
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "importHistoryDismissedPref",
      IMPORT_HISTORY_DISMISSED_PREF,
      false,
      () => {
        this.requestUpdate();
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "hasImportedHistoryPref",
      HAS_IMPORTED_HISTORY_PREF,
      false,
      () => {
        this.requestUpdate();
      }
    );
    if (!this.importHistoryDismissedPref && !this.hasImportedHistoryPrefs) {
      let profileAccessor = await lazy.ProfileAge();
      let profileCreateTime = await profileAccessor.created;
      let timeNow = new Date().getTime();
      let profileAge = timeNow - profileCreateTime;
      // Convert milliseconds to days
      this.profileAge = profileAge / 1000 / 60 / 60 / 24;
    }
  }

  stop() {
    if (!this._started) {
      return;
    }
    this._started = false;
    this.placesQuery.close();

    this.toggleVisibilityInCardContainer();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.stop();
    this.migrationWizardDialog?.removeEventListener(
      "MigrationWizard:Close",
      this.migrationWizardDialog
    );
  }

  async #updateAllHistoryItems(allHistoryItems) {
    if (allHistoryItems) {
      this.allHistoryItems = allHistoryItems;
    } else {
      await this.updateHistoryData();
    }
    this.resetHistoryMaps();
    this.lists.forEach(list => list.requestUpdate());
    await this.#updateSearchResults();
  }

  async #updateSearchResults() {
    if (this.searchQuery) {
      try {
        this.searchResults = await this.placesQuery.searchHistory(
          this.searchQuery,
          SEARCH_RESULTS_LIMIT
        );
      } catch (e) {
        // Connection interrupted, ignore.
      }
    } else {
      this.searchResults = null;
    }
  }

  viewVisibleCallback() {
    this.start();
  }

  viewHiddenCallback() {
    this.stop();
  }

  static queries = {
    cards: { all: "card-container:not([hidden])" },
    migrationWizardDialog: "#migrationWizardDialog",
    emptyState: "fxview-empty-state",
    lists: { all: "fxview-tab-list" },
    showAllHistoryBtn: ".show-all-history-button",
    searchTextbox: "fxview-search-textbox",
    sortInputs: { all: "input[name=history-sort-option]" },
    panelList: "panel-list",
  };

  static properties = {
    ...ViewPage.properties,
    allHistoryItems: { type: Map },
    historyMapByDate: { type: Array },
    historyMapBySite: { type: Array },
    // Making profileAge a reactive property for testing
    profileAge: { type: Number },
    searchResults: { type: Array },
    sortOption: { type: String },
  };

  async getUpdateComplete() {
    await super.getUpdateComplete();
    await Promise.all(Array.from(this.cards).map(card => card.updateComplete));
  }

  async updateHistoryData() {
    this.allHistoryItems = await this.placesQuery.getHistory({
      daysOld: 60,
      limit: lazy.maxRowsPref,
      sortBy: this.sortOption,
    });
  }

  resetHistoryMaps() {
    this.historyMapByDate = [];
    this.historyMapBySite = [];
  }

  createHistoryMaps() {
    if (this.sortOption === "date" && !this.historyMapByDate.length) {
      const {
        visitsFromToday,
        visitsFromYesterday,
        visitsByDay,
        visitsByMonth,
      } = this.placesQuery;

      // Add visits from today and yesterday.
      if (visitsFromToday.length) {
        this.historyMapByDate.push({
          l10nId: "firefoxview-history-date-today",
          items: visitsFromToday,
        });
      }
      if (visitsFromYesterday.length) {
        this.historyMapByDate.push({
          l10nId: "firefoxview-history-date-yesterday",
          items: visitsFromYesterday,
        });
      }

      // Add visits from this month, grouped by day.
      visitsByDay.forEach(visits => {
        this.historyMapByDate.push({
          l10nId: "firefoxview-history-date-this-month",
          items: visits,
        });
      });

      // Add visits from previous months, grouped by month.
      visitsByMonth.forEach(visits => {
        this.historyMapByDate.push({
          l10nId: "firefoxview-history-date-prev-month",
          items: visits,
        });
      });
    } else if (this.sortOption === "site" && !this.historyMapBySite.length) {
      this.historyMapBySite = Array.from(
        this.allHistoryItems.entries(),
        ([domain, items]) => ({
          domain,
          items,
          l10nId: domain ? null : "firefoxview-history-site-localhost",
        })
      ).sort((a, b) => a.domain.localeCompare(b.domain));
    }
  }

  onPrimaryAction(e) {
    // Record telemetry
    Services.telemetry.recordEvent(
      "firefoxview_next",
      "history",
      "visits",
      null,
      {}
    );

    let currentWindow = this.getWindow();
    if (currentWindow.openTrustedLinkIn) {
      let where = lazy.BrowserUtils.whereToOpenLink(
        e.detail.originalEvent,
        false,
        true
      );
      if (where == "current") {
        where = "tab";
      }
      currentWindow.openTrustedLinkIn(e.originalTarget.url, where);
    }
  }

  onSecondaryAction(e) {
    this.triggerNode = e.originalTarget;
    e.target.querySelector("panel-list").toggle(e.detail.originalEvent);
  }

  deleteFromHistory(e) {
    lazy.PlacesUtils.history.remove(this.triggerNode.url);
    this.recordContextMenuTelemetry("delete-from-history", e);
  }

  async onChangeSortOption(e) {
    this.sortOption = e.target.value;
    Services.telemetry.recordEvent(
      "firefoxview_next",
      "sort_history",
      "tabs",
      null,
      {
        sort_type: this.sortOption,
      }
    );
    await this.updateHistoryData();
    await this.#updateSearchResults();
  }

  showAllHistory() {
    // Record telemetry
    Services.telemetry.recordEvent(
      "firefoxview_next",
      "show_all_history",
      "tabs",
      null,
      {}
    );

    // Open History view in Library window
    this.getWindow().PlacesCommandHook.showPlacesOrganizer("History");
  }

  async openMigrationWizard() {
    let migrationWizardDialog = this.migrationWizardDialog;

    if (migrationWizardDialog.open) {
      return;
    }

    await customElements.whenDefined("migration-wizard");

    // If we've been opened before, remove the old wizard and insert a
    // new one to put it back into its starting state.
    if (!migrationWizardDialog.firstElementChild) {
      let wizard = document.createElement("migration-wizard");
      wizard.toggleAttribute("dialog-mode", true);
      migrationWizardDialog.appendChild(wizard);
    }
    migrationWizardDialog.firstElementChild.requestState();

    this.migrationWizardDialog.addEventListener(
      "MigrationWizard:Close",
      function (e) {
        e.currentTarget.close();
      }
    );

    migrationWizardDialog.showModal();
  }

  shouldShowImportBanner() {
    return (
      this.profileAge < 8 &&
      !this.hasImportedHistoryPref &&
      !this.importHistoryDismissedPref
    );
  }

  dismissImportHistory() {
    Services.prefs.setBoolPref(IMPORT_HISTORY_DISMISSED_PREF, true);
  }

  updated() {
    this.fullyUpdated = true;
    if (this.lists?.length) {
      this.toggleVisibilityInCardContainer();
    }
  }

  panelListTemplate() {
    return html`
      <panel-list slot="menu" data-tab-type="history">
        <panel-item
          @click=${this.deleteFromHistory}
          data-l10n-id="firefoxview-history-context-delete"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <hr />
        <panel-item
          @click=${this.openInNewWindow}
          data-l10n-id="fxviewtabrow-open-in-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <panel-item
          @click=${this.openInNewPrivateWindow}
          data-l10n-id="fxviewtabrow-open-in-private-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <hr />
        <panel-item
          @click=${this.copyLink}
          data-l10n-id="fxviewtabrow-copy-link"
          data-l10n-attrs="accesskey"
        ></panel-item>
      </panel-list>
    `;
  }

  /**
   * The template to use for cards-container.
   */
  get cardsTemplate() {
    if (this.searchResults) {
      return this.#searchResultsTemplate();
    } else if (this.allHistoryItems.size) {
      return this.#historyCardsTemplate();
    }
    return this.#emptyMessageTemplate();
  }

  #historyCardsTemplate() {
    let cardsTemplate = [];
    if (this.sortOption === "date" && this.historyMapByDate.length) {
      this.historyMapByDate.forEach(historyItem => {
        if (historyItem.items.length) {
          let dateArg = JSON.stringify({ date: historyItem.items[0].time });
          cardsTemplate.push(html`<card-container>
            <h2
              slot="header"
              data-l10n-id=${historyItem.l10nId}
              data-l10n-args=${dateArg}
            ></h2>
            <fxview-tab-list
              slot="main"
              class="with-context-menu"
              dateTimeFormat=${historyItem.l10nId.includes("prev-month")
                ? "dateTime"
                : "time"}
              hasPopup="menu"
              maxTabsLength=${this.maxTabsLength}
              .tabItems=${historyItem.items}
              @fxview-tab-list-primary-action=${this.onPrimaryAction}
              @fxview-tab-list-secondary-action=${this.onSecondaryAction}
            >
              ${this.panelListTemplate()}
            </fxview-tab-list>
          </card-container>`);
        }
      });
    } else if (this.historyMapBySite.length) {
      this.historyMapBySite.forEach(historyItem => {
        if (historyItem.items.length) {
          cardsTemplate.push(html`<card-container>
            <h3 slot="header" data-l10n-id="${ifDefined(historyItem.l10nId)}">
              ${historyItem.domain}
            </h3>
            <fxview-tab-list
              slot="main"
              class="with-context-menu"
              dateTimeFormat="dateTime"
              hasPopup="menu"
              maxTabsLength=${this.maxTabsLength}
              .tabItems=${historyItem.items}
              @fxview-tab-list-primary-action=${this.onPrimaryAction}
              @fxview-tab-list-secondary-action=${this.onSecondaryAction}
            >
              ${this.panelListTemplate()}
            </fxview-tab-list>
          </card-container>`);
        }
      });
    }
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
    return html` <card-container toggleDisabled>
      <h3
        slot="header"
        data-l10n-id="firefoxview-search-results-header"
        data-l10n-args=${JSON.stringify({
          query: escapeHtmlEntities(this.searchQuery),
        })}
      ></h3>
      ${when(
        this.searchResults.length,
        () =>
          html`<h3
            slot="secondary-header"
            data-l10n-id="firefoxview-search-results-count"
            data-l10n-args="${JSON.stringify({
              count: this.searchResults.length,
            })}"
          ></h3>`
      )}
      <fxview-tab-list
        slot="main"
        class="with-context-menu"
        dateTimeFormat="dateTime"
        hasPopup="menu"
        maxTabsLength="-1"
        .searchQuery=${this.searchQuery}
        .tabItems=${this.searchResults}
        @fxview-tab-list-primary-action=${this.onPrimaryAction}
        @fxview-tab-list-secondary-action=${this.onSecondaryAction}
      >
        ${this.panelListTemplate()}
      </fxview-tab-list>
    </card-container>`;
  }

  render() {
    if (!this.selectedTab) {
      return null;
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/history.css"
      />
      <dialog id="migrationWizardDialog"></dialog>
      <div class="sticky-container bottom-fade">
        <h2
          class="page-header heading-large"
          data-l10n-id="firefoxview-history-header"
        ></h2>
        <div class="history-sort-options">
          ${when(
            isSearchEnabled(),
            () => html` <div class="history-sort-option">
              <fxview-search-textbox
                .query=${this.searchQuery}
                data-l10n-id="firefoxview-search-text-box-history"
                data-l10n-attrs="placeholder"
                .size=${this.searchTextboxSize}
                @fxview-search-textbox-query=${this.onSearchQuery}
              ></fxview-search-textbox>
            </div>`
          )}
          <div class="history-sort-option">
            <input
              type="radio"
              id="sort-by-date"
              name="history-sort-option"
              value="date"
              ?checked=${this.sortOption === "date"}
              @click=${this.onChangeSortOption}
            />
            <label
              for="sort-by-date"
              data-l10n-id="firefoxview-sort-history-by-date-label"
            ></label>
          </div>
          <div class="history-sort-option">
            <input
              type="radio"
              id="sort-by-site"
              name="history-sort-option"
              value="site"
              ?checked=${this.sortOption === "site"}
              @click=${this.onChangeSortOption}
            />
            <label
              for="sort-by-site"
              data-l10n-id="firefoxview-sort-history-by-site-label"
            ></label>
          </div>
        </div>
      </div>
      <div class="cards-container">
        <card-container
          class="import-history-banner"
          hideHeader="true"
          ?hidden=${!this.shouldShowImportBanner()}
        >
          <div slot="main">
            <div class="banner-text">
              <span data-l10n-id="firefoxview-import-history-header"></span>
              <span
                data-l10n-id="firefoxview-import-history-description"
              ></span>
            </div>
            <div class="buttons">
              <button
                class="primary choose-browser"
                data-l10n-id="firefoxview-choose-browser-button"
                @click=${this.openMigrationWizard}
              ></button>
              <button
                class="close ghost-button"
                data-l10n-id="firefoxview-import-history-close-button"
                @click=${this.dismissImportHistory}
              ></button>
            </div>
          </div>
        </card-container>
        ${this.cardsTemplate}
      </div>
      <div
        class="show-all-history-footer"
        ?hidden=${!this.allHistoryItems.size}
      >
        <button
          class="show-all-history-button"
          data-l10n-id="firefoxview-show-all-history"
          @click=${this.showAllHistory}
          ?hidden=${this.searchResults}
        ></button>
      </div>
    `;
  }

  async onSearchQuery(e) {
    this.searchQuery = e.detail.query;
    this.#updateSearchResults();
  }

  willUpdate(changedProperties) {
    this.fullyUpdated = false;
    if (this.allHistoryItems.size && !changedProperties.has("sortOption")) {
      // onChangeSortOption() will update history data once it has been fetched
      // from the API.
      this.createHistoryMaps();
    }
  }
}
customElements.define("view-history", HistoryInView);
