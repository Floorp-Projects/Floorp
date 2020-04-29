/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the throbber is displayed correctly or not.

const TEST_URI = `
  <style>
  body {
    color: blue;
    border-block-color: lime;
    user-modify: read-only;
  }
  div {
    font-variant-alternates: historical-forms;
  }
  </style>
  <body>
    <div>test</div>
  </body>
`;

const {
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START,
} = require("devtools/client/inspector/compatibility/actions/index");

add_task(async function() {
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const {
    allElementsPane,
    inspector,
    selectedElementPane,
  } = await openCompatibilityView();

  info("Check the throbber visibility at the beginning");
  assertThrobber(allElementsPane, false);
  assertThrobber(selectedElementPane, false);

  info("Reload the browsing page");
  const onStart = waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START
  );
  const onComplete = waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE
  );
  gBrowser.reloadTab(tab);

  info("Check the throbber visibility of after starting updating action");
  await onStart;
  assertThrobber(allElementsPane, true);
  assertThrobber(selectedElementPane, false);

  info("Check the throbber visibility of after completing updating action");
  await onComplete;
  assertThrobber(allElementsPane, false);
  assertThrobber(selectedElementPane, false);
});

function assertThrobber(panel, expectedVisibility) {
  const isThrobberVisible = !!panel.querySelector(".devtools-throbber");
  is(
    isThrobberVisible,
    expectedVisibility,
    "Visibility of the throbber is correct"
  );
}
