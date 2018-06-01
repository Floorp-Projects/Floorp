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

const SIDES = ["top", "right", "bottom", "left"];

const X = 32;
const Y = 20;
const WIDTH = 160;
const HEIGHT = 100;
const HYPOTENUSE = Math.hypot(WIDTH, HEIGHT).toFixed(2);

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  const { show, finalize } = helper;

  helper.prefix = PREFIX;

  await show();

  await hasNoLabelsWhenStarts(helper);
  await hasSizeLabelWhenMoved(helper);
  await hasCorrectSizeLabelValue(helper);
  await hasSizeLabelAndGuidesWhenStops(helper);
  await hasCorrectSizeLabelValue(helper);

  await finalize();
});

async function hasNoLabelsWhenStarts({isElementHidden, synthesizeMouse}) {
  info("Checking highlighter has no labels when we start to select");

  await synthesizeMouse({
    selector: ":root",
    options: {type: "mousedown"},
    x: X,
    y: Y
  });

  let hidden = await isElementHidden("label-size");
  ok(hidden, "label's size still hidden");

  hidden = await isElementHidden("label-position");
  ok(hidden, "label's position still hidden");

  info("Checking highlighter has no guides when we start to select");

  let guidesHidden = true;
  for (const side of SIDES) {
    guidesHidden = guidesHidden && (await isElementHidden("guide-" + side));
  }

  ok(guidesHidden, "guides are hidden during dragging");
}

async function hasSizeLabelWhenMoved({isElementHidden, synthesizeMouse}) {
  info("Checking highlighter has size label when we select the area");

  await synthesizeMouse({
    selector: ":root",
    options: {type: "mousemove"},
    x: X + WIDTH,
    y: Y + HEIGHT
  });

  let hidden = await isElementHidden("label-size");
  is(hidden, false, "label's size is visible during selection");

  hidden = await isElementHidden("label-position");
  ok(hidden, "label's position still hidden");

  info("Checking highlighter has no guides when we select the area");

  let guidesHidden = true;
  for (const side of SIDES) {
    guidesHidden = guidesHidden && (await isElementHidden("guide-" + side));
  }

  ok(guidesHidden, "guides are hidden during selection");
}

async function hasSizeLabelAndGuidesWhenStops({isElementHidden, synthesizeMouse}) {
  info("Checking highlighter has size label and guides when we stop");

  await synthesizeMouse({
    selector: ":root",
    options: {type: "mouseup"},
    x: X + WIDTH,
    y: Y + HEIGHT
  });

  let hidden = await isElementHidden("label-size");
  is(hidden, false, "label's size is visible when the selection is done");

  hidden = await isElementHidden("label-position");
  ok(hidden, "label's position still hidden");

  let guidesVisible = true;
  for (const side of SIDES) {
    guidesVisible = guidesVisible && !(await isElementHidden("guide-" + side));
  }

  ok(guidesVisible, "guides are visible when the selection is done");
}

async function hasCorrectSizeLabelValue({getElementTextContent}) {
  const text = await getElementTextContent("label-size");

  const [width, height, hypot] = text.match(/\d.*px/g);

  is(parseFloat(width), WIDTH, "width on label's size is correct");
  is(parseFloat(height), HEIGHT, "height on label's size is correct");
  is(parseFloat(hypot), HYPOTENUSE, "hypotenuse on label's size is correct");
}
