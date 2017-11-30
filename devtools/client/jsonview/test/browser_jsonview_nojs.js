/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  info("Test JSON without JavaScript started.");

  let oldPref = SpecialPowers.getBoolPref("javascript.enabled");
  SpecialPowers.setBoolPref("javascript.enabled", false);

  const TEST_JSON_URL = "data:application/json,[1,2,3]";
  yield addJsonViewTab(TEST_JSON_URL, 0).catch(() => {
    info("JSON Viewer did not load");
    return executeInContent("Test:JsonView:GetElementVisibleText", {selector: "html"})
    .then(result => {
      info("Checking visible text contents.");
      is(result.text, "[1,2,3]", "The raw source should be visible.");
    });
  });

  SpecialPowers.setBoolPref("javascript.enabled", oldPref);
});
