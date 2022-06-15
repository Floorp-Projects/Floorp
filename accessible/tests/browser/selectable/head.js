/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported testMultiSelectable */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
/* import-globals-from ../../mochitest/selectable.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR },
  { name: "selectable.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR }
);

// Handle case where multiple selection change events are coalesced into
// a SELECTION_WITHIN event. Promise resolves to true in that case.
function multipleSelectionChanged(widget, changedChildren, selected) {
  return Promise.race([
    Promise.all(
      changedChildren.map(id =>
        waitForStateChange(id, STATE_SELECTED, selected)
      )
    ).then(() => false),
    waitForEvent(EVENT_SELECTION_WITHIN, widget).then(() => true),
  ]);
}

async function testMultiSelectable(widget, selectableChildren, msg = "") {
  let isRemote = false;
  try {
    widget.DOMNode;
  } catch (e) {
    isRemote = true;
  }

  testSelectableSelection(widget, [], `${msg}: initial`);

  let promise = waitForStateChange(selectableChildren[0], STATE_SELECTED, true);
  widget.addItemToSelection(0);
  await promise;
  testSelectableSelection(
    widget,
    [selectableChildren[0]],
    `${msg}: addItemToSelection(0)`
  );

  promise = waitForStateChange(selectableChildren[0], STATE_SELECTED, false);
  widget.removeItemFromSelection(0);
  await promise;
  testSelectableSelection(widget, [], `${msg}: removeItemFromSelection(0)`);

  promise = multipleSelectionChanged(widget, selectableChildren, true);
  let success = widget.selectAll();
  ok(success, `${msg}: selectAll success`);
  await promise;
  if (isRemote && isCacheEnabled) {
    await untilCacheIs(
      () => widget.selectedItemCount,
      selectableChildren.length,
      "Selection cache updated"
    );
  }
  testSelectableSelection(widget, selectableChildren, `${msg}: selectAll`);

  promise = multipleSelectionChanged(widget, selectableChildren, false);
  widget.unselectAll();
  await promise;
  if (isRemote && isCacheEnabled) {
    await untilCacheIs(
      () => widget.selectedItemCount,
      0,
      "Selection cache updated"
    );
  }
  testSelectableSelection(widget, [], `${msg}: selectAll`);
}
