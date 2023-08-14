/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PlacesQuery } from "resource://gre/modules/PlacesQuery.sys.mjs";

/**
 * Extension of PlacesQuery which provides additional caches for Firefox View.
 */
export class FirefoxViewPlacesQuery extends PlacesQuery {
  /** @type {Date} */
  #todaysDate = null;
  /** @type {Date} */
  #yesterdaysDate = null;

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

  /**
   * Get a list of visits per day for each day on this month, excluding today
   * and yesterday.
   *
   * @returns {HistoryVisit[][]}
   *   A list of visits for each day.
   */
  get visitsByDay() {
    const visitsPerDay = [];
    for (const [time, visits] of this.cachedHistory.entries()) {
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
   * @returns {HistoryVisit[][]}
   *   A list of visits for each month.
   */
  get visitsByMonth() {
    const visitsPerMonth = [];
    let previousMonth = null;
    for (const [time, visits] of this.cachedHistory.entries()) {
      const date = new Date(time);
      if (
        this.#isSameDate(date, this.#yesterdaysDate) ||
        this.#isSameMonth(date, this.#todaysDate)
      ) {
        continue;
      }
      const month = this.getStartOfMonthTimestamp(date);
      if (month !== previousMonth) {
        visitsPerMonth.push(visits);
      } else {
        visitsPerMonth[visitsPerMonth.length - 1] =
          visitsPerMonth[visitsPerMonth.length - 1].concat(visits);
      }
      previousMonth = month;
    }
    return visitsPerMonth;
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
    visit.secondaryL10nId = "fxviewtabrow-options-menu-button";
    visit.secondaryL10nArgs = JSON.stringify({
      tabTitle: visit.title || visit.url,
    });
  }

  async fetchHistory() {
    await super.fetchHistory();
    if (this.cachedHistoryOptions.sortBy === "date") {
      this.#setTodaysDate();
    }
  }

  handlePageVisited(event) {
    const visit = super.handlePageVisited(event);
    if (!visit) {
      return;
    }
    if (this.cachedHistoryOptions.sortBy === "date") {
      this.#setTodaysDate();
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
