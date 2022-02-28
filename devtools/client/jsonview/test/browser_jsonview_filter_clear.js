/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "array_json.json";

function click(selector) {
  return BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    {},
    gBrowser.selectedBrowser
  );
}

add_task(async function() {
  info("Test filter input is cleared when pressing the clear button");

  await addJsonViewTab(TEST_JSON_URL);

  // Type "honza" in the filter input
  const count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 6, "There must be expected number of rows");
  await sendString("honza", ".jsonPanelBox .searchBox");
  await waitForFilter();

  const filterInputValue = await getFilterInputValue();
  is(filterInputValue, "honza", "Filter input shoud be filled");

  // Check the json is filtered
  const hiddenCount = await getElementCount(
    ".jsonPanelBox .treeTable .treeRow.hidden"
  );
  is(hiddenCount, 4, "There must be expected number of hidden rows");

  // Click on the clear filter input button
  await click("button.devtools-searchinput-clear");

  // Check the json is not filtered and the filter input is empty
  const newfilterInputValue = await getFilterInputValue();
  is(newfilterInputValue, "", "Filter input should be empty");
  const newCount = await getElementCount(
    ".jsonPanelBox .treeTable .treeRow.hidden"
  );
  is(newCount, 0, "There must be expected number of rows");
});

function getFilterInputValue() {
  return getElementAttr(".devtools-filterinput", "value");
}
