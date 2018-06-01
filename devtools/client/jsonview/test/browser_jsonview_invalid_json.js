/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "invalid_json.json";

add_task(async function() {
  info("Test invalid JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  const count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  ok(count == 0, "There must be no row");

  const text = await getElementText(".jsonPanelBox .jsonParseError");
  ok(text, "There must be an error description");
});
