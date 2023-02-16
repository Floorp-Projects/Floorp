/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that loader and new tab appear when pretty printing,
// and the selected location is mapped afterwards

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js", 4, 8);
  is(
    dbg.selectors.getSourceCount(),
    1,
    "There is only one source before pretty printing"
  );

  await prettyPrint(dbg);
  info("Wait for a second tab to be displayed with the pretty printed source");
  is(
    countTabs(dbg),
    2,
    "Two tabs are opened, one for minified and another one for pretty"
  );
  info("Wait for the pretty printed source to be selected on a different line");
  await waitForSelectedLocation(dbg, 5);
  is(
    dbg.selectors.getSourceCount(),
    2,
    "The is two sources after pretty printing"
  );

  info("Re select the minified version");
  await selectSource(dbg, "pretty.js", 4, 8);
  info("Re toggle pretty print from the minified source");
  await prettyPrint(dbg);
  is(countTabs(dbg), 2, "There is stil two tabs");
  info(
    "Wait for re-selecting the mapped location in the pretty printed source"
  );
  await waitForSelectedLocation(dbg, 5);
  is(
    dbg.selectors.getSourceCount(),
    2,
    "There is still 2 sources after retrying to pretty print the same source"
  );
});
