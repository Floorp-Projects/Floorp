/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a component used to sort matches in a UrlbarQueryContext.
 */

var EXPORTED_SYMBOLS = ["UrlbarMuxerUnifiedComplete"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Places.Urlbar.UrlbarMuxerUnifiedComplete"));

const RESULT_TYPE_TO_GROUP = new Map([
  [ UrlbarUtils.RESULT_TYPE.TAB_SWITCH, UrlbarUtils.MATCH_GROUP.GENERAL ],
  [ UrlbarUtils.RESULT_TYPE.SEARCH, UrlbarUtils.MATCH_GROUP.SUGGESTION ],
  [ UrlbarUtils.RESULT_TYPE.URL, UrlbarUtils.MATCH_GROUP.GENERAL ],
  [ UrlbarUtils.RESULT_TYPE.KEYWORD, UrlbarUtils.MATCH_GROUP.GENERAL ],
  [ UrlbarUtils.RESULT_TYPE.OMNIBOX, UrlbarUtils.MATCH_GROUP.EXTENSION ],
  [ UrlbarUtils.RESULT_TYPE.REMOTE_TAB, UrlbarUtils.MATCH_GROUP.GENERAL ],
]);

/**
 * Class used to create a muxer.
 * The muxer receives and sorts matches in a UrlbarQueryContext.
 */
class MuxerUnifiedComplete extends UrlbarMuxer {
  constructor() {
    super();
  }

  get name() {
    return "UnifiedComplete";
  }

  /**
   * Sorts matches in the given UrlbarQueryContext.
   * @param {UrlbarQueryContext} context The query context.
   */
  sort(context) {
    if (!context.results.length) {
      return;
    }
    // Check the first match, if it's a preselected search match, use search buckets.
    let firstMatch = context.results[0];
    let buckets = context.preselected &&
                  firstMatch.type == UrlbarUtils.RESULT_TYPE.SEARCH ?
                    UrlbarPrefs.get("matchBucketsSearch") :
                    UrlbarPrefs.get("matchBuckets");
    logger.debug(`Buckets: ${buckets}`);
    let sortedMatches = [];
    let handled = new Set();
    for (let [group, count] of buckets) {
      // Search all the available matches and fill this bucket.
      for (let match of context.results) {
        if (count == 0) {
          // There's no more space in this bucket.
          break;
        }
        if (handled.has(match)) {
          // Already handled.
          continue;
        }

        // Handle the heuristic result.
        if (group == UrlbarUtils.MATCH_GROUP.HEURISTIC &&
            match == firstMatch && context.preselected) {
          sortedMatches.push(match);
          handled.add(match);
          count--;
        } else if (group == RESULT_TYPE_TO_GROUP.get(match.type)) {
          sortedMatches.push(match);
          handled.add(match);
          count--;
        } else if (!RESULT_TYPE_TO_GROUP.has(match.type)) {
          let errorMsg = `Result type ${match.type} is not mapped to a match group.`;
          logger.error(errorMsg);
          Cu.reportError(errorMsg);
        }
      }
    }
    context.results = sortedMatches;
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
