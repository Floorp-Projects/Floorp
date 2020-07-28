/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI */

this.matchPatterns = class extends ExtensionAPI {
  getAPI(context) {
    return {
      matchPatterns: {
        getMatcher(patterns) {
          const set = new MatchPatternSet(patterns);
          return Cu.cloneInto(
            {
              matches: url => {
                return set.matches(url);
              },
            },
            context.cloneScope,
            {
              cloneFunctions: true,
            }
          );
        },
      },
    };
  }
};
