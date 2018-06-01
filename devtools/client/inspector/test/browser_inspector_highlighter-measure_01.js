/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = `data:text/html;charset=utf-8,
                    <div style='
                        position:absolute;
                        left: 0;
                        top: 0;
                        width: 40000px;
                        height: 8000px'>
                    </div>`;

const PREFIX = "measuring-tool-highlighter-";
const HIGHLIGHTER_TYPE = "MeasuringToolHighlighter";

const X = 32;
const Y = 20;

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  const { finalize } = helper;

  helper.prefix = PREFIX;

  await isHiddenByDefault(helper);
  await areLabelsHiddenByDefaultWhenShows(helper);
  await areLabelsProperlyDisplayedWhenMouseMoved(helper);

  await finalize();
});

async function isHiddenByDefault({isElementHidden}) {
  info("Checking the highlighter is hidden by default");

  let hidden = await isElementHidden("root");
  ok(hidden, "highlighter's root is hidden by default");

  hidden = await isElementHidden("label-size");
  ok(hidden, "highlighter's label size is hidden by default");

  hidden = await isElementHidden("label-position");
  ok(hidden, "highlighter's label position is hidden by default");
}

async function areLabelsHiddenByDefaultWhenShows({isElementHidden, show}) {
  info("Checking the highlighter is displayed when asked");

  await show();

  let hidden = await isElementHidden("elements");
  is(hidden, false, "highlighter is visible after show");

  hidden = await isElementHidden("label-size");
  ok(hidden, "label's size still hidden");

  hidden = await isElementHidden("label-position");
  ok(hidden, "label's position still hidden");
}

async function areLabelsProperlyDisplayedWhenMouseMoved({isElementHidden,
  synthesizeMouse, getElementTextContent}) {
  info("Checking labels are properly displayed when mouse moved");

  await synthesizeMouse({
    selector: ":root",
    options: {type: "mousemove"},
    x: X,
    y: Y
  });

  let hidden = await isElementHidden("label-position");
  is(hidden, false, "label's position is displayed after the mouse is moved");

  hidden = await isElementHidden("label-size");
  ok(hidden, "label's size still hidden");

  const text = await getElementTextContent("label-position");

  const [x, y] = text.replace(/ /g, "").split(/\n/);

  is(+x, X, "label's position shows the proper X coord");
  is(+y, Y, "label's position shows the proper Y coord");
}
