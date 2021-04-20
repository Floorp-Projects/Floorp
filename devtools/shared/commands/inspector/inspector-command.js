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
