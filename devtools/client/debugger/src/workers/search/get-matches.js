/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import assert from "../../utils/assert";
import buildQuery from "../../utils/build-query";

export default function getMatches(query, text, options) {
  if (!query || !text || !options) {
    return [];
  }
  const regexQuery = buildQuery(query, options, {
    isGlobal: true,
  });
  const matchedLocations = [];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    let singleMatch;
    const line = lines[i];
    while ((singleMatch = regexQuery.exec(line)) !== null) {
      // Flow doesn't understand the test above.
      if (!singleMatch) {
        throw new Error("no singleMatch");
      }

      matchedLocations.push({
        line: i,
        ch: singleMatch.index,
        match: singleMatch[0],
      });

      // When the match is an empty string the regexQuery.lastIndex will not
      // change resulting in an infinite loop so we need to check for this and
      // increment it manually in that case.  See issue #7023
      if (singleMatch[0] === "") {
        assert(
          !regexQuery.unicode,
          "lastIndex++ can cause issues in unicode mode"
        );
        regexQuery.lastIndex++;
      }
    }
  }
  return matchedLocations;
}
