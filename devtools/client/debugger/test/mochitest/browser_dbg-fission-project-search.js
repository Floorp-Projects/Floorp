/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const TEST_COM_URI = `${URL_ROOT_COM_SSL}examples/doc_dbg-fission-frame-sources.html`;

// Testing project search for remote frames.
add_task(async function () {
  // Load page and wait for sources. simple.js is loaded from
  // the top-level document in the dot com domain, while simple2.js
  // is loaded from the remote frame in the dot org domain
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );

  pressKey(dbg, "projectSearch");
  type(dbg, "foo");
  pressKey(dbg, "Enter");

  await waitForSearchResults(dbg, 2);

  const fileResults = findAllElements(dbg, "projectSearchFileResults");
  const matches = findAllElements(dbg, "projectSearchExpandedResults");

  is(fileResults.length, 2, "Two results found");
  is(matches.length, 6, "Total no of matches found");

  // Asserts that we find a matches in the js file included in the top-level document
  assertFileResult("simple1.js", 5);
  // Asserts that we find the match in the js file included
  assertFileResult("simple2.js", 1);

  function assertFileResult(fileMatched, noOfMatches) {
    // The results can be out of order so let find it from the collection
    const match = [...fileResults].find(result =>
      result.querySelector(".file-path").innerText.includes(fileMatched)
    );

    ok(match, `Matches were found in ${fileMatched} file.`);

    const matchText = noOfMatches > 1 ? "matches" : "match";
    is(
      match.querySelector(".matches-summary").innerText.trim(),
      `(${noOfMatches} ${matchText})`,
      `${noOfMatches} ${matchText} were found in ${fileMatched} file.`
    );
  }
});
