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

add_task(function*() {
  let helper = yield openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  let { finalize } = helper;

  helper.prefix = PREFIX;

  yield isHiddenByDefault(helper);
  yield areLabelsHiddenByDefaultWhenShows(helper);
  yield areLabelsProperlyDisplayedWhenMouseMoved(helper);

  yield finalize();
});

function* isHiddenByDefault({isElementHidden}) {
  info("Checking the highlighter is hidden by default");

  let hidden = yield isElementHidden("elements");
  ok(hidden, "highlighter's root is hidden by default");

  hidden = yield isElementHidden("label-size");
  ok(hidden, "highlighter's label size is hidden by default");

  hidden = yield isElementHidden("label-position");
  ok(hidden, "highlighter's label position is hidden by default");
}

function* areLabelsHiddenByDefaultWhenShows({isElementHidden, show}) {
  info("Checking the highlighter is displayed when asked");

  yield show();

  let hidden = yield isElementHidden("elements");
  is(hidden, false, "highlighter is visible after show");

  hidden = yield isElementHidden("label-size");
  ok(hidden, "label's size still hidden");

  hidden = yield isElementHidden("label-position");
  ok(hidden, "label's position still hidden");
}

function* areLabelsProperlyDisplayedWhenMouseMoved({isElementHidden,
  synthesizeMouse, getElementTextContent}) {
  info("Checking labels are properly displayed when mouse moved");

  yield synthesizeMouse({
    selector: ":root",
    options: {type: "mousemove"},
    x: X,
    y: Y
  });

  let hidden = yield isElementHidden("label-position");
  is(hidden, false, "label's position is displayed after the mouse is moved");

  hidden = yield isElementHidden("label-size");
  ok(hidden, "label's size still hidden");

  let text = yield getElementTextContent("label-position");

  let [x, y] = text.replace(/ /g, "").split(/\n/);

  is(+x, X, "label's position shows the proper X coord");
  is(+y, Y, "label's position shows the proper Y coord");
}
