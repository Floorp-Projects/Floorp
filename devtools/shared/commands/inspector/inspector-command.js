/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class InspectorCommand {
  constructor({ commands }) {
    this.commands = commands;
  }

  /**
   * Return the list of all current target's inspector fronts
   *
   * @return {Promise<Array<InspectorFront>>}
   */
  async getAllInspectorFronts() {
    return this.commands.targetCommand.getAllFronts(
      this.commands.targetCommand.TYPES.FRAME,
      "inspector"
    );
  }

  /**
   * Search the document for the given string and return all the results.
   *
   * @param {Object} walkerFront
   * @param {String} query
   *        The string to search for.
   * @param {Object} options
   *        {Boolean} options.reverse - search backwards
   * @returns {Array} The list of search results
   */
  async walkerSearch(walkerFront, query, options = {}) {
    const result = await walkerFront.search(query, options);
    return result.list.items();
  }

  /**
   * Incrementally search the top-level document and sub frames for a given string.
   * Only one result is sent back at a time. Calling the
   * method again with the same query will send the next result.
   * If a new query which does not match the current one all is reset and new search
   * is kicked off.
   *
   * @param {String} query
   *         The string / selector searched for
   * @param {Object} options
   *        {Boolean} reverse - determines if the search is done backwards
   * @returns {Object} res
   *          {String} res.type
   *          {String} res.query - The string / selector searched for
   *          {Object} res.node - the current node
   *          {Number} res.resultsIndex - The index of the current node
   *          {Number} res.resultsLength - The total number of results found.
   */
  async findNextNode(query, { reverse } = {}) {
    const inspectors = await this.getAllInspectorFronts();
    const nodes = await Promise.all(
      inspectors.map(({ walker }) =>
        this.walkerSearch(walker, query, { reverse })
      )
    );
    const results = nodes.flat();

    // If the search query changes
    if (this._searchQuery !== query) {
      this._searchQuery = query;
      this._currentIndex = -1;
    }

    if (!results.length) {
      return null;
    }

    this._currentIndex = reverse
      ? this._currentIndex - 1
      : this._currentIndex + 1;

    if (this._currentIndex >= results.length) {
      this._currentIndex = 0;
    }
    if (this._currentIndex < 0) {
      this._currentIndex = results.length - 1;
    }

    return {
      node: results[this._currentIndex],
      resultsIndex: this._currentIndex,
      resultsLength: results.length,
    };
  }

  /**
   * Returns a list of matching results for CSS selector autocompletion.
   *
   * @param {String} query
   *        The selector query being completed
   * @param {String} firstPart
   *        The exact token being completed out of the query
   * @param {String} state
   *        One of "pseudo", "id", "tag", "class", "null"
   * @return {Array<string>} suggestions
   *        The list of suggested CSS selectors
   */
  async getSuggestionsForQuery(query, firstPart, state) {
    // Get all inspectors where we want suggestions from.
    const inspectors = await this.getAllInspectorFronts();

    const mergedSuggestions = [];
    // Get all of the suggestions.
    await Promise.all(
      inspectors.map(async ({ walker }) => {
        const { suggestions } = await walker.getSuggestionsForQuery(
          query,
          firstPart,
          state
        );
        for (const [suggestion, count, type] of suggestions) {
          // Merge any already existing suggestion with the new one, by incrementing the count
          // which is the second element of the array.
          const existing = mergedSuggestions.find(
            ([s, , t]) => s == suggestion && t == type
          );
          if (existing) {
            existing[1] += count;
          } else {
            mergedSuggestions.push([suggestion, count, type]);
          }
        }
      })
    );

    // Descending sort the list by count, i.e. second element of the arrays
    return sortSuggestions(mergedSuggestions);
  }
}

// This is a fork of the server sort:
// https://searchfox.org/mozilla-central/rev/46a67b8656ac12b5c180e47bc4055f713d73983b/devtools/server/actors/inspector/walker.js#1447
function sortSuggestions(suggestions) {
  const sorted = suggestions.sort((a, b) => {
    // Computed a sortable string with first the inverted count, then the name
    let sortA = 10000 - a[1] + a[0];
    let sortB = 10000 - b[1] + b[0];

    // Prefixing ids, classes and tags, to group results
    const firstA = a[0].substring(0, 1);
    const firstB = b[0].substring(0, 1);

    const getSortKeyPrefix = firstLetter => {
      if (firstLetter === "#") {
        return "2";
      }
      if (firstLetter === ".") {
        return "1";
      }
      return "0";
    };

    sortA = getSortKeyPrefix(firstA) + sortA;
    sortB = getSortKeyPrefix(firstB) + sortB;

    // String compare
    return sortA.localeCompare(sortB);
  });
  return sorted.slice(0, 25);
}

module.exports = InspectorCommand;
