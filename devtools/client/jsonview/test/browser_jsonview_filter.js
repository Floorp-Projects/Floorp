/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "array_json.json";

add_task(async function() {
  info("Test valid JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  const count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 6, "There must be expected number of rows");

  // XXX use proper shortcut to focus the filter box
  // as soon as bug Bug 1178771 is fixed.
  await sendString("h", ".jsonPanelBox .searchBox");

  // The filtering is done asynchronously so, we need to wait.
  await waitForFilter();

  const hiddenCount = await getElementCount(
    ".jsonPanelBox .treeTable .treeRow.hidden"
  );
  is(hiddenCount, 4, "There must be expected number of hidden rows");
});
