/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PlacesQuery } from "resource://gre/modules/PlacesQuery.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BinarySearch: "resource://gre/modules/BinarySearch.sys.mjs",
});

/**
 * Extension of PlacesQuery which provides additional caches for Firefox View.
 */
export class FirefoxViewPlacesQuery extends PlacesQuery {
  /** @type {Map<number, HistoryVisit[]>} */
  visitsByDay = new Map();
  /** @type {Map<number, HistoryVisit[]>} */
  visitsByMonth = new Map();

  /** @type {Date} */
  #todaysDate = null;
  /** @type {Date} */
  #yesterdaysDate = null;

  initializeCache(options) {
    super.initializeCache(options);
    this.#resetDateCaches();
  }

  close() {
    super.close();
    this.#resetDateCaches();
  }

  get visitsFromToday() {
    if (this.cachedHistory == null || this.#todaysDate == null) {
      return [];
    }
    const mapKey = this.getStartOfDayTimestamp(this.#todaysDate);
    return this.cachedHistory.has(mapKey) ? this.cachedHistory.get(mapKey) : [];
  }

  get visitsFromYesterday() {
    if (this.cachedHistory == null || this.#yesterdaysDate == null) {
      return [];
    }
    const mapKey = this.getStartOfDayTimestamp(this.#yesterdaysDate);
    return this.cachedHistory.has(mapKey) ? this.cachedHistory.get(mapKey) : [];
  }

  #resetDateCaches() {
    this.visitsByDay.clear();
    this.visitsByMonth.clear();
    this.#todaysDate = null;
    this.#yesterdaysDate = null;
  }

  appendToCache(visit) {
    super.appendToCache(visit);
    this.#normalizeVisit(visit);
  }

  insertSortedIntoCache(visit) {
    super.insertSortedIntoCache(visit);
    this.#normalizeVisit(visit);
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
    visit.secondaryL10nId = "fxviewtabrow-open-menu-button";
  }

  async fetchHistory() {
    await super.fetchHistory();
    if (this.cachedHistoryOptions.sortBy === "date") {
      this.#buildDateContainers();
    }
  }

  #buildDateContainers() {
    this.#setTodaysDate();
    this.cachedHistory.forEach((visits, time) => {
      const date = new Date(time);
      if (
        this.#isSameDate(date, this.#todaysDate) ||
        this.#isSameDate(date, this.#yesterdaysDate)
      ) {
        // Getters for visits from today/yesterday will handle this.
        return;
      }
      if (this.#isSameMonth(date, this.#todaysDate)) {
        this.visitsByDay.set(time, visits);
      } else {
        const mapKey = this.getStartOfMonthTimestamp(date);
        if (!this.visitsByMonth.has(mapKey)) {
          this.visitsByMonth.set(mapKey, []);
        }
        const container = this.visitsByMonth.get(mapKey);
        this.visitsByMonth.set(mapKey, container.concat(visits));
      }
    });
  }

  handlePageVisited(event) {
    const visit = super.handlePageVisited(event);
    if (!visit) {
      return;
    }
    if (this.cachedHistoryOptions.sortBy === "date") {
      if (this.#todaysDate == null) {
        this.#setTodaysDate();
      }
      if (
        visit.date.getTime() > this.#todaysDate.getTime() &&
        !this.#isSameDate(visit.date, this.#todaysDate)
      ) {
        // Today's date has passed, thus we need to rebuild date caches.
        this.#resetDateCaches();
        this.#buildDateContainers();
        return;
      }
      if (
        this.#isSameDate(visit.date, this.#todaysDate) ||
        this.#isSameDate(visit.date, this.#yesterdaysDate)
      ) {
        // Getters for visits from today/yesterday will handle this.
        return;
      }
      if (this.#isSameMonth(visit.date, this.#todaysDate)) {
        const mapKey = this.getStartOfDayTimestamp(visit.date);
        this.visitsByDay.set(mapKey, this.cachedHistory.get(mapKey));
      } else {
        const mapKey = this.getStartOfMonthTimestamp(visit.date);
        if (!this.visitsByMonth.has(mapKey)) {
          this.visitsByMonth.set(mapKey, []);
        }
        const container = this.visitsByMonth.get(mapKey);
        const insertionPoint = lazy.BinarySearch.insertionIndexOf(
          (a, b) => b.date.getTime() - a.date.getTime(),
          container,
          visit
        );
        container.splice(insertionPoint, 0, visit);
      }
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
}
