/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineLazyGetter(this, "jsonViewStrings", () => {
  return Services.strings.createBundle(
    "chrome://devtools/locale/jsonview.properties"
  );
});

const TEST_JSON_URL = URL_ROOT + "array_json.json";
const EXPAND_THRESHOLD = 100 * 1024;

add_task(async function () {
  info("Test expand/collapse small JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  /* Initial sanity check */
  const countBefore = await getElementCount(".treeRow");
  is(countBefore, 6, "There must be six rows");

  /* Test the "Collapse All" button */
  let selector = ".jsonPanelBox .toolbar button.collapse";
  await clickJsonNode(selector);
  let countAfter = await getElementCount(".treeRow");
  is(countAfter, 3, "There must be three rows");

  /* Test the "Expand All" button */
  selector = ".jsonPanelBox .toolbar button.expand";
  is(
    await getElementText(selector),
    jsonViewStrings.GetStringFromName("jsonViewer.ExpandAll"),
    "Expand button doesn't warn that the action will be slow"
  );
  await clickJsonNode(selector);
  countAfter = await getElementCount(".treeRow");
  is(countAfter, 6, "There must be six expanded rows");
});

add_task(async function () {
  info("Test expand button for big JSON started");

  const json = JSON.stringify({
    data: Array(1e5)
      .fill()
      .map(x => "hoot"),
    status: "ok",
  });
  ok(
    json.length > EXPAND_THRESHOLD,
    "The generated JSON must be larger than 100kB"
  );
  await addJsonViewTab("data:application/json," + json);
  is(
    await getElementText(".jsonPanelBox .toolbar button.expand"),
    jsonViewStrings.GetStringFromName("jsonViewer.ExpandAllSlow"),
    "Expand button warns that the action will be slow"
  );
});
