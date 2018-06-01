/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test JSON without JavaScript started.");

  const oldPref = SpecialPowers.getBoolPref("javascript.enabled");
  SpecialPowers.setBoolPref("javascript.enabled", false);

  const TEST_JSON_URL = "data:application/json,[1,2,3]";

  // "uninitialized" will be the last app readyState because JS is disabled.
  await addJsonViewTab(TEST_JSON_URL, {appReadyState: "uninitialized"});

  info("Checking visible text contents.");
  const {text} = await executeInContent("Test:JsonView:GetElementVisibleText",
    {selector: "html"});
  is(text, "[1,2,3]", "The raw source should be visible.");

  SpecialPowers.setBoolPref("javascript.enabled", oldPref);
});
