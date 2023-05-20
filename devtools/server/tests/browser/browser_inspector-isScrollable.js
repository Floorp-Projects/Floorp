/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const URL = MAIN_DOMAIN + "inspector-isScrollable-data.html";

const CASES = [
  { id: "body", expected: false },
  { id: "no_children", expected: false },
  { id: "one_child_no_overflow", expected: false },
  { id: "margin_left_overflow", expected: true },
  { id: "transform_overflow", expected: true },
  { id: "nested_overflow", expected: true },
  { id: "intermediate_overflow", expected: true },
  { id: "multiple_overflow_at_different_depths", expected: true },
  { id: "overflow_hidden", expected: false },
  { id: "scrollbar_none", expected: false },
];

add_task(async function () {
  info(
    "Test that elements with scrollbars have a true value for isScrollable, and elements without scrollbars have a false value."
  );
  const { walker } = await initInspectorFront(URL);

  for (const { id, expected } of CASES) {
    info(`Checking element id ${id}.`);

    const el = await walker.querySelector(walker.rootNode, `#${id}`);
    is(el.isScrollable, expected, `${id} has expected value for isScrollable.`);
  }
});
