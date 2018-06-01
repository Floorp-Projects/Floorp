/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "array_json.json";
const EXPAND_THRESHOLD = 100 * 1024;

add_task(async function() {
  info("Test expand/collapse JSON started");

  await addJsonViewTab(TEST_JSON_URL);
  const browser = gBrowser.selectedBrowser;

  /* Initial sanity check */
  const countBefore = await getElementCount(".treeRow");
  ok(countBefore == 6, "There must be six rows");

  /* Test the "Collapse All" button */
  let selector = ".jsonPanelBox .toolbar button.collapse";
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, browser);
  let countAfter = await getElementCount(".treeRow");
  ok(countAfter == 3, "There must be three rows");

  /* Test the "Expand All" button */
  selector = ".jsonPanelBox .toolbar button.expand";
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, browser);
  countAfter = await getElementCount(".treeRow");
  ok(countAfter == 6, "There must be six expanded rows");

  /* Test big file handling */
  const json = JSON.stringify({data: Array(1e5).fill().map(x => "hoot"), status: "ok"});
  ok(json.length > EXPAND_THRESHOLD, "The generated JSON must be larger than 100kB");
  await addJsonViewTab("data:application/json," + json);
  ok(document.querySelector(selector) == null, "The Expand All button must be gone");
});
