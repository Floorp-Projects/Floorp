/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test JSON with NUL started.");

  const TEST_JSON_URL = "data:application/json,\"foo_%00_bar\"";
  await addJsonViewTab(TEST_JSON_URL);

  await selectJsonViewContentTab("rawdata");
  const rawData = await getElementText(".textPanelBox .data");
  is(rawData, "\"foo_\u0000_bar\"",
     "The NUL character has been preserved.");
});
