/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "csp_json.json";

add_task(async function() {
  info("Test CSP JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  let count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 1, "There must be one row");
});
