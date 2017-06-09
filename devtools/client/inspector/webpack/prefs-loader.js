/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Rewrite devtools.js or all.js, leaving just the relevant pref() calls.

"use strict";

const PREF_WHITELIST = [
  "devtools",
  "layout.css.grid.enabled"
];

const acceptLine = function (line) {
  let matches = line.match(/^ *pref\("([^"]+)"/);
  if (!matches || !matches[1]) {
    return false;
  }

  let [, prefName] = matches;
  return PREF_WHITELIST.some(filter => prefName.startsWith(filter));
};

module.exports = function (content) {
  this.cacheable && this.cacheable();

  // If we're reading devtools.js we have to do some preprocessing.
  // If we're reading all.js we just assume we can dump all the
  // conditionals.
  let isDevtools = this.request.endsWith("/devtools.js");

  // This maps the text of a "#if" to its truth value.  This has to
  // cover all uses of #if in devtools.js.
  const ifMap = {
    "#if MOZ_UPDATE_CHANNEL == beta": false,
    "#if defined(NIGHTLY_BUILD)": false,
    "#ifdef NIGHTLY_BUILD": false,
    "#ifdef MOZ_DEV_EDITION": false,
    "#ifdef RELEASE_OR_BETA": true,
    "#ifdef RELEASE_BUILD": true,
  };

  let lines = content.split("\n");
  let ignoring = false;
  let newLines = [];
  let continuation = false;
  for (let line of lines) {
    if (line.startsWith("sticky_pref")) {
      line = line.slice(7);
    }

    if (isDevtools) {
      if (line.startsWith("#if")) {
        if (!(line in ifMap)) {
          throw new Error("missing line in ifMap: " + line);
        }
        ignoring = !ifMap[line];
      } else if (line.startsWith("#else")) {
        ignoring = !ignoring;
      } else if (line.startsWith("#endif")) {
        ignoring = false;
      }
    }

    if (continuation || (!ignoring && acceptLine(line))) {
      newLines.push(line);

      // The call to pref(...); might span more than one line.
      continuation = !/\);/.test(line);
    }
  }
  return newLines.join("\n");
};
