/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test JSON with slash started.");

  const TEST_JSON_URL = "data:application/json,{\"a/b\":[1,2],\"a\":{\"b\":[3,4]}}";
  await addJsonViewTab(TEST_JSON_URL);

  const countBefore = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(countBefore, 7, "There must be seven rows");
});
