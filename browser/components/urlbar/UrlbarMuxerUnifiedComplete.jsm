/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a component used to sort matches in a QueryContext.
 */

var EXPORTED_SYMBOLS = ["UrlbarMuxerUnifiedComplete"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Places.Urlbar.UrlbarMuxerUnifiedComplete"));

const MATCH_TYPE_TO_GROUP = new Map([
  [ UrlbarUtils.MATCH_TYPE.TAB_SWITCH, UrlbarUtils.MATCH_GROUP.GENERAL ],
  [ UrlbarUtils.MATCH_TYPE.SEARCH, UrlbarUtils.MATCH_GROUP.SUGGESTION ],
  [ UrlbarUtils.MATCH_TYPE.URL, UrlbarUtils.MATCH_GROUP.GENERAL ],
  [ UrlbarUtils.MATCH_TYPE.KEYWORD, UrlbarUtils.MATCH_GROUP.GENERAL ],
  [ UrlbarUtils.MATCH_TYPE.OMNIBOX, UrlbarUtils.MATCH_GROUP.EXTENSION ],
  [ UrlbarUtils.MATCH_TYPE.REMOTE_TAB, UrlbarUtils.MATCH_GROUP.GENERAL ],
]);

/**
 * Class used to create a muxer.
 * The muxer receives and sorts matches in a QueryContext.
 */
class MuxerUnifiedComplete {
  constructor() {
    // Nothing.
  }

  get name() {
    return "MuxerUnifiedComplete";
  }

  /**
   * Sorts matches in the given QueryContext.
   * @param {object} context a QueryContext
   */
  sort(context) {
    if (!context.results.length) {
      return;
    }
    // Check the first match, if it's a preselected search match, use search buckets.
    let firstMatch = context.results[0];
    let buckets = context.preselected &&
                  firstMatch.type == UrlbarUtils.MATCH_TYPE.SEARCH ?
                    UrlbarPrefs.get("matchBucketsSearch") :
                    UrlbarPrefs.get("matchBuckets");
    logger.debug(`Buckets: ${buckets}`);
    let sortedMatches = [];
    for (let [group, count] of buckets) {
      // Search all the available matches and fill this bucket.
      for (let match of context.results) {
        if (count == 0) {
          // There's no more space in this bucket.
          break;
        }

        // Handle the heuristic result.
        if (group == UrlbarUtils.MATCH_GROUP.HEURISTIC &&
            match == firstMatch && context.preselected) {
          sortedMatches.push(match);
          count--;
        } else if (group == MATCH_TYPE_TO_GROUP.get(match.type)) {
          sortedMatches.push(match);
          count--;
        } else if (!MATCH_TYPE_TO_GROUP.has(match.type)) {
          let errorMsg = `Match type ${match.type} is not mapped to a match group.`;
          logger.error(errorMsg);
          Cu.reportError(errorMsg);
        }
      }
    }
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
