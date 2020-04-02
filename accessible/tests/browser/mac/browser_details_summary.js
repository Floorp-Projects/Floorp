/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

/**
 * Test details/summary
 */
addAccessibleTask(
  `<details id="details"><summary>Foo</summary><p>Bar</p></details>`,
  async (browser, accDoc) => {
    let details = getNativeInterface(accDoc, "details");
    is(
      details.getAttributeValue("AXRole"),
      "AXGroup",
      "Correct role for details"
    );
    is(
      details.getAttributeValue("AXSubrole"),
      "AXDetails",
      "Correct subrole for details"
    );

    let detailsChildren = details.getAttributeValue("AXChildren");
    is(detailsChildren.length, 1, "collapsed details has only one child");

    let summary = detailsChildren[0];
    is(
      summary.getAttributeValue("AXRole"),
      "AXButton",
      "Correct role for summary"
    );
    is(
      summary.getAttributeValue("AXSubrole"),
      "AXSummary",
      "Correct subrole for summary"
    );
    is(summary.getAttributeValue("AXExpanded"), 0, "Summary is collapsed");

    let actions = summary.actionNames;
    ok(actions.includes("AXPress"), "Summary Has press action");

    let reorder = waitForEvent(EVENT_REORDER, "details");
    summary.performAction("AXPress");
    is(summary.getAttributeValue("AXExpanded"), 1, "Summary is collapsed");
    // The reorder gecko event notifies us of a tree change.
    await reorder;

    detailsChildren = details.getAttributeValue("AXChildren");
    is(detailsChildren.length, 2, "collapsed details has only one child");
  }
);
