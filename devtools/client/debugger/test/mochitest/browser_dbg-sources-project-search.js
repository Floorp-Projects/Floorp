/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing project search for various types of sources scenarios

"use strict";

requestLongerTimeout(3);

// Tests for searching in dynamic (without urls) sources
add_task(async function testSearchDynamicScripts() {
  const dbg = await initDebugger("doc-minified.html");

  const executeComplete = dbg.commands.scriptCommand.execute(
    `const foo = 5; debugger; console.log(foo)`
  );
  await waitForPaused(dbg);

  await openProjectSearch(dbg);
  const fileResults = await doProjectSearch(dbg, "foo", 1);
  ok(
    /source\d+/g.test(fileResults[0].innerText),
    "The search result was found in the eval script."
  );

  await resume(dbg);
  await executeComplete;
});

// Tests that minified sources are ignored when the prettyfied versions
// exist.
add_task(async function testIgnoreMinifiedSourceForPrettySource() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await openProjectSearch(dbg);
  let fileResults = await doProjectSearch(dbg, "stuff", 1);

  is(fileResults.length, 1, "Only the result was found");
  ok(
    fileResults[0].innerText.includes("pretty.js\n(1 match)"),
    "The search result was found in the minified (pretty.js) source"
  );

  await selectSource(dbg, "pretty.js");
  await waitForSelectedSource(dbg, "pretty.js");

  info("Pretty print the source");
  await prettyPrint(dbg);

  fileResults = await doProjectSearch(dbg, "stuff", 2);

  is(
    fileResults.length,
    2,
    "Two results were found form both the pretty and minified sources"
  );
  ok(
    fileResults[0].innerText.includes("pretty.js:formatted\n(1 match)"),
    "The first search result was found in the prettyified (pretty.js:formatted) source"
  );

  ok(
    fileResults[1].innerText.includes("pretty.js\n(1 match)"),
    "The second search result was found in the minified (pretty.js) source"
  );
});

// Test the prioritization of files. Original files should be prioritized over
// generated files and third-party files should be deprioritized and load after the others.
add_task(async function testFilesPrioritization() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await openProjectSearch(dbg);
  const fileResults = await doProjectSearch(dbg, "componentDidMount", 3);

  is(getExpandedResultsCount(dbg), 13);

  ok(
    fileResults[0].innerText.includes(
      "browser/devtools/client/debugger/test/mochitest/examples/react/build/App.js"
    ),
    "The first item should be the original (prettified) file"
  );

  ok(
    findAllElements(dbg, "projectSearchExpandedResults")[0].innerText.endsWith(
      "componentDidMount() {"
    ),
    "The first result match in the original file is correct"
  );

  const firstNodeModulesResultFound = [...fileResults].findIndex(el =>
    el.innerText.includes("node_modules")
  );
  is(
    firstNodeModulesResultFound,
    2,
    "The node_modules (third-party) file is the last result in the list"
  );
});

add_task(async function testBlackBoxedSources() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");
  await selectSource(dbg, "pretty.js");

  info("Blackbox pretty.js");
  await toggleBlackbox(dbg);

  await openProjectSearch(dbg);
  let fileResults = await doProjectSearch(dbg, "stuff", 0);

  is(fileResults.length, 0, "No results were found as pretty.js is blackboxed");

  info("Unblackbox pretty.js");
  await toggleBlackbox(dbg);

  await openProjectSearch(dbg);
  fileResults = await doProjectSearch(dbg, "stuff", 1);

  is(
    fileResults.length,
    1,
    "One result was found as pretty.js is no longer blackboxed"
  );
});

async function toggleBlackbox(dbg) {
  await clickElement(dbg, "blackbox");
  await Promise.any([
    waitForDispatch(dbg.store, "BLACKBOX_WHOLE_SOURCES"),
    waitForDispatch(dbg.store, "UNBLACKBOX_WHOLE_SOURCES"),
  ]);
}
