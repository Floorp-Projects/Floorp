/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that configuring two different highlighters to autohide
// will not overwrite each other's timers.
add_task(async function () {
  info("Loading the test document and opening the inspector");
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<p id='one'>TEST 1</p>"
  );

  const HALF_SECOND = 500;
  const nodeFront = await getNodeFront("#one", inspector);

  const waitForShowEvents = waitForNEvents(
    inspector.highlighters,
    "highlighter-shown",
    2
  );
  const waitForHideEvents = waitForNEvents(
    inspector.highlighters,
    "highlighter-hidden",
    2
  );

  info("Show Box Model Highlighter, then hide after half a second");
  inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    nodeFront,
    { duration: HALF_SECOND }
  );

  info("Show Selector Highlighter, then hide after half a second");
  inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.SELECTOR,
    nodeFront,
    { selector: "#one", duration: HALF_SECOND }
  );

  info("Waiting for 2 highlighter-shown and 2 highlighter-hidden events");
  await Promise.all([waitForShowEvents, waitForHideEvents]);
});
