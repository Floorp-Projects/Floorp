/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that source maps work in the event popup.

const INITIAL_URL = URL_ROOT + "doc_markup_void_elements.html";
const TEST_URL = URL_ROOT + "doc_markup_events-source_map.html";

/* import-globals-from helper_events_test_runner.js */
loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "#clicky",
    isSourceMapped: true,
    expected: [
      {
        type: "click",
        filename: "webpack:///events_original.js:7",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: `function clickme() {
  console.log("clickme");
}`,
      },
    ],
  },
];

add_task(async function() {
  // Load some other URL before opening the toolbox, then navigate to
  // the test URL.  This ensures that source map service will see the
  // sources as they are loaded, avoiding any races.
  const {toolbox, inspector, testActor} = await openInspectorForURL(INITIAL_URL);

  // Ensure the source map service is operating.  This looks a bit
  // funny, but sourceMapURLService is a getter, and we don't need the
  // result.
  toolbox.sourceMapURLService;

  await navigateTo(inspector, TEST_URL);

  await inspector.markup.expandAll();

  for (const test of TEST_DATA) {
    await checkEventsForNode(test, inspector, testActor);
  }

  // Wait for promises to avoid leaks when running this as a single test.
  // We need to do this because we have opened a bunch of popups and don't them
  // to affect other test runs when they are GCd.
  await promiseNextTick();
});
