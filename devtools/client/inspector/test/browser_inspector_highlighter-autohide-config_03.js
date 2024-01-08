/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getTimeoutMultiplier } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/AppInfo.sys.mjs"
);

// Test that configuring a highlighter to autohide twice
// will replace the first timer and hide just once.
add_task(async function () {
  info("Loading the test document and opening the inspector");
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<p id='one'>TEST 1</p>"
  );

  // On opt builds, use 500ms and 1000ms for popup timeouts.
  // On debug builds, multiply the first timeout by the platform multiplier to avoid
  // an extra `hidden` event between the `showHighlighterTypeForNode` commands.
  const FIRST_POPUP_TIMEOUT = getTimeoutMultiplier() * 500;
  const SECOND_POPUP_TIMEOUT = 1000;

  const nodeFront = await getNodeFront("#one", inspector);

  const waitForShowEvents = waitForNEvents(
    inspector.highlighters,
    "highlighter-shown",
    2
  );
  const waitForHideEvents = waitForNEvents(
    inspector.highlighters,
    "highlighter-hidden",
    1
  );

  info("Show Box Model Highlighter, then hide after half a second");
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    nodeFront,
    { duration: FIRST_POPUP_TIMEOUT }
  );

  info("Show Box Model Highlighter again, then hide after one second");
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    nodeFront,
    { duration: SECOND_POPUP_TIMEOUT }
  );

  info("Waiting for 2 highlighter-shown and 1 highlighter-hidden event");
  await Promise.all([waitForShowEvents, waitForHideEvents]);

  /*
  Since the second duration passed is longer than the first and is supposed to overwrite
  the first, it is reasonable to expect that the "highlighter-hidden" event was emitted
  after the second (longer) duration expired. As an added check, we naively wait for an
  additional time amounting to the sum of both durations to check if the first timer was
  somehow not overwritten and fires another "highlighter-hidden" event.
   */
  let wasEmitted = false;
  const waitForExtraEvent = new Promise((resolve, reject) => {
    const _handler = () => {
      wasEmitted = true;
      resolve();
    };

    inspector.highlighters.on("highlighter-hidden", _handler, { once: true });
  });

  info("Wait to see if another highlighter-hidden event is emitted");
  await Promise.race([
    waitForExtraEvent,
    wait(FIRST_POPUP_TIMEOUT + SECOND_POPUP_TIMEOUT),
  ]);

  is(wasEmitted, false, "An extra highlighter-hidden event was not emitted");
});
