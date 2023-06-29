/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";

const { PlacesQuery } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesQuery.sys.mjs"
);
const { BrowserUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/BrowserUtils.sys.mjs"
);

function getWindow() {
  return window.browsingContext.embedderWindowGlobal.browsingContext.window;
}

class HistoryInView extends ViewPage {
  constructor() {
    super();
    this.allHistoryItems = [];
    this.historyMapByDate = [];
    this.historyMapBySite = [];
    // Setting maxTabsLength to -1 for no max
    this.maxTabsLength = -1;
    this.placesQuery = new PlacesQuery();
    this.sortOption = "date";
  }

  async connectedCallback() {
    super.connectedCallback();
    this.updateHistoryData();
    this.placesQuery.observeHistory(newHistory => {
      this.allHistoryItems = [...newHistory];
      this.resetHistoryMaps();
    });
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.placesQuery.close();
  }

  static queries = {
    cards: { all: "card-container" },
  };

  static properties = {
    ...ViewPage.properties,
    allHistoryItems: { type: Array },
    historyMapByDate: { type: Array },
    historyMapBySite: { type: Array },
  };

  async getUpdateComplete() {
    await super.getUpdateComplete();
    await Promise.all(Array.from(this.cards).map(card => card.updateComplete));
  }

  async updateHistoryData(newHistoryData) {
    if (!newHistoryData) {
      this.allHistoryItems = await this.placesQuery.getHistory({ daysOld: 60 });
    } else {
      this.allHistoryItems = newHistoryData;
      // Reset history maps before sorting, normalizing, and creating updated maps
      this.resetHistoryMaps();
    }
  }

  resetHistoryMaps() {
    this.historyMapByDate = [];
    this.historyMapBySite = [];
  }

  sortHistoryData() {
    if (this.sortOption == "date" && !this.historyMapByDate.length) {
      this.allHistoryItems.sort((a, b) => {
        return new Date(b.date) - new Date(a.date);
      });
    } else if (!this.historyMapBySite.length) {
      this.allHistoryItems.sort((a, b) => {
        return BrowserUtils.formatURIStringForDisplay(a.url.toLowerCase()) >
          BrowserUtils.formatURIStringForDisplay(b.url.toLowerCase())
          ? 1
          : -1;
      });
    }
    return this.allHistoryItems;
  }

  normalizeHistoryData() {
    // Normalize data for fxview-tabs-list
    this.allHistoryItems.forEach(historyItem => {
      historyItem.time = historyItem.date.getTime();
      historyItem.title = historyItem.title
        ? historyItem.title
        : historyItem.url;
      historyItem.icon = `page-icon:${historyItem.url}`;
      historyItem.primaryL10nId = "fxviewtabrow-tabs-list-tab";
      historyItem.primaryL10nArgs = JSON.stringify({
        targetURI: historyItem.url,
      });
      historyItem.secondaryL10nId = "fxviewtabrow-open-menu-button";
    });
  }

  isDateToday(dateObj) {
    const today = new Date();
    return (
      dateObj.getDate() === today.getDate() &&
      dateObj.getMonth() === today.getMonth() &&
      dateObj.getFullYear() === today.getFullYear()
    );
  }

  isDateYesterday(dateObj) {
    const today = new Date();
    const yesterday = new Date(today);
    yesterday.setDate(yesterday.getDate() - 1);

    return (
      dateObj.getDate() === yesterday.getDate() &&
      dateObj.getMonth() === today.getMonth() &&
      dateObj.getFullYear() === today.getFullYear()
    );
  }

  isDateThisMonth(dateObj) {
    const today = new Date();
    return (
      !this.isDateToday(dateObj) &&
      !this.isDateYesterday(dateObj) &&
      dateObj.getMonth() === today.getMonth() &&
      dateObj.getFullYear() === today.getFullYear()
    );
  }

  isDatePrevMonth(dateObj) {
    const today = new Date();
    return (
      dateObj.getFullYear() === today.getFullYear() &&
      dateObj.getMonth() !== today.getMonth()
    );
  }

  createHistoryMaps() {
    if (this.sortOption === "date" && !this.historyMapByDate.length) {
      let todayItems = this.allHistoryItems.filter(historyItem =>
        this.isDateToday(historyItem.date)
      );
      if (todayItems.length) {
        this.historyMapByDate.push({
          l10nId: "firefoxview-history-date-today",
          items: todayItems,
        });
      }
      let yesterdayItems = this.allHistoryItems.filter(historyItem =>
        this.isDateYesterday(historyItem.date)
      );
      if (yesterdayItems.length) {
        this.historyMapByDate.push({
          l10nId: "firefoxview-history-date-yesterday",
          items: yesterdayItems,
        });
      }
      let historyThisMonth = this.allHistoryItems.filter(historyItem =>
        this.isDateThisMonth(historyItem.date)
      );
      historyThisMonth.forEach(historyItem => {
        let historyAtDayThisMonth = historyThisMonth.filter(
          historyItemThisMonth =>
            historyItemThisMonth.date.getDate() === historyItem.date.getDate()
        );
        if (historyAtDayThisMonth.length) {
          this.historyMapByDate.push({
            l10nId: "firefoxview-history-date-this-month",
            items: historyAtDayThisMonth,
          });
          historyThisMonth = historyThisMonth.filter(
            historyItemThisMonth =>
              historyItemThisMonth.date.getDate() !== historyItem.date.getDate()
          );
        }
      });

      let historyOlderThanThisMonth = this.allHistoryItems.filter(historyItem =>
        this.isDatePrevMonth(historyItem.date)
      );
      historyOlderThanThisMonth.forEach(historyItem => {
        let items = historyOlderThanThisMonth.filter(
          historyItemOlderThanThisMonth =>
            historyItemOlderThanThisMonth.date.getMonth() ===
            historyItem.date.getMonth()
        );
        if (items.length) {
          this.historyMapByDate.push({
            l10nId: "firefoxview-history-date-prev-month",
            items,
          });
          historyOlderThanThisMonth = historyOlderThanThisMonth.filter(
            historyItemOlderThanThisMonth =>
              historyItemOlderThanThisMonth.date.getMonth() !==
              historyItem.date.getMonth()
          );
        }
      });
    } else if (!this.historyMapBySite.length) {
      let allHistoryCopy = [...this.allHistoryItems];
      allHistoryCopy.forEach(historyItem => {
        let domain = BrowserUtils.formatURIStringForDisplay(historyItem.url);
        let items = allHistoryCopy.filter(
          historyCopyItem =>
            BrowserUtils.formatURIStringForDisplay(historyCopyItem.url) ===
            domain
        );
        if (items.length) {
          this.historyMapBySite.push({
            domain,
            items,
          });
          allHistoryCopy = allHistoryCopy.filter(
            historyCopyItem =>
              BrowserUtils.formatURIStringForDisplay(historyCopyItem.url) !==
              domain
          );
        }
      });
    }
  }

  onPrimaryAction(e) {
    let currentWindow = getWindow();
    if (currentWindow.openTrustedLinkIn) {
      let where = BrowserUtils.whereToOpenLink(
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
    e.target.querySelector("panel-list").toggle(e.detail.originalEvent);
  }

  async toggleSortOption(e) {
    this.sortOption = e.target.value;
    this.updateHistoryData();
  }

  showAllHistory() {
    // Open History view in Library window
    getWindow().PlacesCommandHook.showPlacesOrganizer("History");
  }

  panelListTemplate() {
    return html`
      <panel-list slot="menu">
        <panel-item data-l10n-id="fxviewtabrow-delete"></panel-item>
        <panel-item
          data-l10n-id="fxviewtabrow-forget-about-this-site"
        ></panel-item>
        <hr />
        <panel-item data-l10n-id="fxviewtabrow-open-in-window"></panel-item>
        <panel-item
          data-l10n-id="fxviewtabrow-open-in-private-window"
        ></panel-item>
        <hr />
        <panel-item data-l10n-id="fxviewtabrow-add-bookmark"></panel-item>
        <panel-item data-l10n-id="fxviewtabrow-save-to-pocket"></panel-item>
        <panel-item data-l10n-id="fxviewtabrow-copy-link"></panel-item>
      </panel-list>
    `;
  }

  historyCardsTemplate() {
    let cardsTemplate = [];
    if (this.sortOption === "date" && this.historyMapByDate.length) {
      this.historyMapByDate.forEach((historyItem, index) => {
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
              class="history"
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
            <h2 slot="header">${historyItem.domain}</h2>
            <fxview-tab-list
              slot="main"
              class="history"
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

  emptyMessageTemplate() {
    // TO-DO: Bug 1826604 - Add History empty states and banner
    return html`
      <card-container hideHeader="true" class"empty-state history">
        <p slot="main">EMPTY MESSAGE</p>
      </card-container>
    `;
  }

  render() {
    if (!this.selectedTab) {
      return null;
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview-next.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/history.css"
      />
      <div class="sticky-container bottom-fade">
        <h2 class="page-header" data-l10n-id="firefoxview-history-header"></h2>
        <span class="history-sort-options">
          <div class="history-sort-option">
            <input
              type="radio"
              id="sort-by-date"
              name="history-sort-option"
              value="date"
              checked
              @click=${this.toggleSortOption}
            />
            <label
              for="sort-by-date"
              data-l10n-id="firefoxview-sort-history-by-date-label"
            ></label>
          </div>
          <div class="history-sort-option">
            <input
              type="radio"
              name="history-sort-option"
              value="site"
              @click=${this.toggleSortOption}
            />
            <label
              for="sort-by-site"
              data-l10n-id="firefoxview-sort-history-by-site-label"
            ></label>
          </div>
        </span>
      </div>
      <div class="cards-container">
        ${!this.allHistoryItems.length
          ? this.emptyMessageTemplate()
          : this.historyCardsTemplate()}
      </div>
      <div class="show-all-history-footer">
        <span
          class="show-all-history-link"
          data-l10n-id="firefoxview-show-all-history"
          @click=${this.showAllHistory}
        ></span>
      </div>
    `;
  }

  willUpdate() {
    this.sortHistoryData();
    this.normalizeHistoryData();
    if (this.allHistoryItems.length) {
      this.createHistoryMaps();
    }
  }
}
customElements.define("view-history", HistoryInView);
