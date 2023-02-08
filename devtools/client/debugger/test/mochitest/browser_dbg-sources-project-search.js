/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing project search for various types of sources scenarios

"use strict";

// Tests for searching in dynamic (without urls) sources
add_task(async function testSearchDynamicScripts() {
  const dbg = await initDebugger("doc-minified.html");

  const executeComplete = dbg.commands.scriptCommand.execute(
    `const foo = 5; debugger; console.log(foo)`
  );
  await waitForPaused(dbg);

  await openProjectSearch(dbg);
  type(dbg, "foo");
  pressKey(dbg, "Enter");

  const fileResults = await waitForSearchResults(dbg, 1);

  ok(
    /source\d+/g.test(fileResults[0].innerText),
    "The search result was found in the eval script."
  );

  await closeProjectSearch(dbg);
  await resume(dbg);
  await executeComplete;
});

// Tests that minified sources are ignored when the prettyfied versions
// exist.
add_task(async function testIgnoreMinifiedSourceForPrettySource() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await openProjectSearch(dbg);
  let fileResults = await doProjectSearch(dbg, "stuff");

  is(fileResults.length, 1, "Only the result was found");
  ok(
    fileResults[0].innerText.includes("pretty.js\n(1 match)"),
    "The search result was found in the minified (pretty.js) source"
  );

  await closeProjectSearch(dbg);

  await selectSource(dbg, "pretty.js");
  await waitForSelectedSource(dbg, "pretty.js");

  info("Pretty print the source");
  await prettyPrint(dbg);

  await openProjectSearch(dbg);
  fileResults = await doProjectSearch(dbg, "stuff");

  is(fileResults.length, 1, "Only one result was found");
  ok(
    fileResults[0].innerText.includes("pretty.js:formatted\n(1 match)"),
    "The search result was found in the prettyified (pretty.js:formatted) source"
  );
});

// Test the prioritization of source-mapped files. (Bug 1642778)
add_task(async function testOriginalFilesAsPrioritizedOverGeneratedFiles() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await openProjectSearch(dbg);
  const fileResults = await doProjectSearch(dbg, "componentDidMount");

  is(getExpandedResultsCount(dbg), 8);

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
});
