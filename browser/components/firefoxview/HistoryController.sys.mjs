/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { getLogger } from "chrome://browser/content/firefoxview/helpers.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesQuery: "resource://gre/modules/PlacesQuery.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
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

const HISTORY_MAP_L10N_IDS = {
  sidebar: {
    "history-date-today": "sidebar-history-date-today",
    "history-date-yesterday": "sidebar-history-date-yesterday",
    "history-date-this-month": "sidebar-history-date-this-month",
    "history-date-prev-month": "sidebar-history-date-prev-month",
  },
  firefoxview: {
    "history-date-today": "firefoxview-history-date-today",
    "history-date-yesterday": "firefoxview-history-date-yesterday",
    "history-date-this-month": "firefoxview-history-date-this-month",
    "history-date-prev-month": "firefoxview-history-date-prev-month",
  },
};

/**
 * A list of visits displayed on a card.
 *
 * @typedef {object} CardEntry
 *
 * @property {string} domain
 * @property {HistoryVisit[]} items
 * @property {string} l10nId
 */

export class HistoryController {
  /**
   * @type {{ entries: CardEntry[]; searchQuery: string; sortOption: string; }}
   */
  historyCache;
  host;
  searchQuery;
  sortOption;
  #todaysDate;
  #yesterdaysDate;

  constructor(host, options) {
    this.placesQuery = new lazy.PlacesQuery();
    this.searchQuery = "";
    this.sortOption = "date";
    this.searchResultsLimit = options?.searchResultsLimit || 300;
    this.component = HISTORY_MAP_L10N_IDS?.[options?.component]
      ? options?.component
      : "firefoxview";
    this.historyCache = {
      entries: [],
      searchQuery: null,
      sortOption: null,
    };
    this.host = host;

    host.addController(this);
  }

  hostConnected() {
    this.placesQuery.observeHistory(historyMap => this.updateCache(historyMap));
  }

  hostDisconnected() {
    ChromeUtils.idleDispatch(() => this.placesQuery.close());
  }

  deleteFromHistory() {
    lazy.PlacesUtils.history.remove(this.host.triggerNode.url);
  }

  onSearchQuery(e) {
    this.searchQuery = e.detail.query;
    this.updateCache();
  }

  onChangeSortOption(e) {
    this.sortOption = e.target.value;
    this.updateCache();
  }

  get historyVisits() {
    return this.historyCache.entries;
  }

  get searchResults() {
    return this.historyCache.searchQuery
      ? this.historyCache.entries[0].items
      : null;
  }

  get totalVisitsCount() {
    return this.historyVisits.reduce(
      (count, entry) => count + entry.items.length,
      0
    );
  }

  get isHistoryEmpty() {
    return !this.historyVisits.length;
  }

  /**
   * Update cached history.
   *
   * @param {Map<CacheKey, HistoryVisit[]>} [historyMap]
   *   If provided, performs an update using the given data (instead of fetching
   *   it from the db).
   */
  async updateCache(historyMap) {
    const { searchQuery, sortOption } = this;
    const entries = searchQuery
      ? await this.#getVisitsForSearchQuery(searchQuery)
      : await this.#getVisitsForSortOption(sortOption, historyMap);
    if (this.searchQuery !== searchQuery || this.sortOption !== sortOption) {
      // This query is stale, discard results and do not update the cache / UI.
      return;
    }
    for (const { items } of entries) {
      for (const item of items) {
        this.#normalizeVisit(item);
      }
    }
    this.historyCache = { entries, searchQuery, sortOption };
    this.host.requestUpdate();
  }

  /**
   * Normalize data for fxview-tabs-list.
   *
   * @param {HistoryVisit} visit
   *   The visit to format.
   */
  #normalizeVisit(visit) {
    visit.time = visit.date.getTime();
    visit.title = visit.title || visit.url;
    visit.icon = `page-icon:${visit.url}`;
    visit.primaryL10nId = "fxviewtabrow-tabs-list-tab";
    visit.primaryL10nArgs = JSON.stringify({
      targetURI: visit.url,
    });
    visit.secondaryL10nId = "fxviewtabrow-options-menu-button";
    visit.secondaryL10nArgs = JSON.stringify({
      tabTitle: visit.title || visit.url,
    });
  }

  async #getVisitsForSearchQuery(searchQuery) {
    let items = [];
    try {
      items = await this.placesQuery.searchHistory(
        searchQuery,
        this.searchResultsLimit
      );
    } catch (e) {
      getLogger("HistoryController").warn(
        "There is a new search query in progress, so cancelling this one.",
        e
      );
    }
    return [{ items }];
  }

  async #getVisitsForSortOption(sortOption, historyMap) {
    if (!historyMap) {
      historyMap = await this.#fetchHistory();
    }
    switch (sortOption) {
      case "date":
        this.#setTodaysDate();
        return this.#getVisitsForDate(historyMap);
      case "site":
        return this.#getVisitsForSite(historyMap);
      default:
        return [];
    }
  }

  #setTodaysDate() {
    const now = new Date();
    this.#todaysDate = new Date(
      now.getFullYear(),
      now.getMonth(),
      now.getDate()
    );
    this.#yesterdaysDate = new Date(
      now.getFullYear(),
      now.getMonth(),
      now.getDate() - 1
    );
  }

  /**
   * Get a list of visits, sorted by date, in reverse chronological order.
   *
   * @param {Map<number, HistoryVisit[]>} historyMap
   * @returns {CardEntry[]}
   */
  #getVisitsForDate(historyMap) {
    const entries = [];
    const visitsFromToday = this.#getVisitsFromToday(historyMap);
    const visitsFromYesterday = this.#getVisitsFromYesterday(historyMap);
    const visitsByDay = this.#getVisitsByDay(historyMap);
    const visitsByMonth = this.#getVisitsByMonth(historyMap);

    // Add visits from today and yesterday.
    if (visitsFromToday.length) {
      entries.push({
        l10nId: HISTORY_MAP_L10N_IDS[this.component]["history-date-today"],
        items: visitsFromToday,
      });
    }
    if (visitsFromYesterday.length) {
      entries.push({
        l10nId: HISTORY_MAP_L10N_IDS[this.component]["history-date-yesterday"],
        items: visitsFromYesterday,
      });
    }

    // Add visits from this month, grouped by day.
    visitsByDay.forEach(visits => {
      entries.push({
        l10nId: HISTORY_MAP_L10N_IDS[this.component]["history-date-this-month"],
        items: visits,
      });
    });

    // Add visits from previous months, grouped by month.
    visitsByMonth.forEach(visits => {
      entries.push({
        l10nId: HISTORY_MAP_L10N_IDS[this.component]["history-date-prev-month"],
        items: visits,
      });
    });
    return entries;
  }

  #getVisitsFromToday(cachedHistory) {
    const mapKey = this.placesQuery.getStartOfDayTimestamp(this.#todaysDate);
    const visits = cachedHistory.get(mapKey) ?? [];
    return [...visits];
  }

  #getVisitsFromYesterday(cachedHistory) {
    const mapKey = this.placesQuery.getStartOfDayTimestamp(
      this.#yesterdaysDate
    );
    const visits = cachedHistory.get(mapKey) ?? [];
    return [...visits];
  }

  /**
   * Get a list of visits per day for each day on this month, excluding today
   * and yesterday.
   *
   * @param {Map<number, HistoryVisit[]>} cachedHistory
   *   The history cache to process.
   * @returns {HistoryVisit[][]}
   *   A list of visits for each day.
   */
  #getVisitsByDay(cachedHistory) {
    const visitsPerDay = [];
    for (const [time, visits] of cachedHistory.entries()) {
      const date = new Date(time);
      if (
        this.#isSameDate(date, this.#todaysDate) ||
        this.#isSameDate(date, this.#yesterdaysDate)
      ) {
        continue;
      } else if (!this.#isSameMonth(date, this.#todaysDate)) {
        break;
      } else {
        visitsPerDay.push(visits);
      }
    }
    return visitsPerDay;
  }

  /**
   * Get a list of visits per month for each month, excluding this one, and
   * excluding yesterday's visits if yesterday happens to fall on the previous
   * month.
   *
   * @param {Map<number, HistoryVisit[]>} cachedHistory
   *   The history cache to process.
   * @returns {HistoryVisit[][]}
   *   A list of visits for each month.
   */
  #getVisitsByMonth(cachedHistory) {
    const visitsPerMonth = [];
    let previousMonth = null;
    for (const [time, visits] of cachedHistory.entries()) {
      const date = new Date(time);
      if (
        this.#isSameMonth(date, this.#todaysDate) ||
        this.#isSameDate(date, this.#yesterdaysDate)
      ) {
        continue;
      }
      const month = this.placesQuery.getStartOfMonthTimestamp(date);
      if (month !== previousMonth) {
        visitsPerMonth.push(visits);
      } else {
        visitsPerMonth[visitsPerMonth.length - 1] = visitsPerMonth
          .at(-1)
          .concat(visits);
      }
      previousMonth = month;
    }
    return visitsPerMonth;
  }

  /**
   * Given two date instances, check if their dates are equivalent.
   *
   * @param {Date} dateToCheck
   * @param {Date} date
   * @returns {boolean}
   *   Whether both date instances have equivalent dates.
   */
  #isSameDate(dateToCheck, date) {
    return (
      dateToCheck.getDate() === date.getDate() &&
      this.#isSameMonth(dateToCheck, date)
    );
  }

  /**
   * Given two date instances, check if their months are equivalent.
   *
   * @param {Date} dateToCheck
   * @param {Date} month
   * @returns {boolean}
   *   Whether both date instances have equivalent months.
   */
  #isSameMonth(dateToCheck, month) {
    return (
      dateToCheck.getMonth() === month.getMonth() &&
      dateToCheck.getFullYear() === month.getFullYear()
    );
  }

  /**
   * Get a list of visits, sorted by site, in alphabetical order.
   *
   * @param {Map<string, HistoryVisit[]>} historyMap
   * @returns {CardEntry[]}
   */
  #getVisitsForSite(historyMap) {
    return Array.from(historyMap.entries(), ([domain, items]) => ({
      domain,
      items,
      l10nId: domain ? null : "firefoxview-history-site-localhost",
    })).sort((a, b) => a.domain.localeCompare(b.domain));
  }

  async #fetchHistory() {
    return this.placesQuery.getHistory({
      daysOld: 60,
      limit: lazy.maxRowsPref,
      sortBy: this.sortOption,
    });
  }
}
