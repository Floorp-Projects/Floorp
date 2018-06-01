/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "manifest_json.json";

add_task(async function() {
  info("Test manifest JSON file started");

  await addJsonViewTab(TEST_JSON_URL);

  const count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 37, "There must be expected number of rows");
});
