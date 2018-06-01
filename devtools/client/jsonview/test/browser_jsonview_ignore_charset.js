/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test ignored charset parameter started");

  const encodedChar = "%E2%9D%A4"; // In UTF-8 this is a heavy black heart
  const result = "\u2764"; // ❤
  const TEST_JSON_URL = "data:application/json;charset=ANSI," + encodedChar;

  await addJsonViewTab(TEST_JSON_URL);
  await selectJsonViewContentTab("rawdata");

  const text = await getElementText(".textPanelBox .data");
  is(text, result, "The charset parameter is ignored and UTF-8 is used.");
});
