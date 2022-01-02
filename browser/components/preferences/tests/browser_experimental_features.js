/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testPrefRequired() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", false]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let experimentalCategory = doc.getElementById("category-experimental");
  ok(experimentalCategory, "The category exists");
  ok(experimentalCategory.hidden, "The category is hidden");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testCanOpenWithPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let experimentalCategory = doc.getElementById("category-experimental");
  ok(experimentalCategory, "The category exists");
  ok(!experimentalCategory.hidden, "The category is not hidden");

  let categoryHeader = await TestUtils.waitForCondition(
    () => doc.getElementById("firefoxExperimentalCategory"),
    "Waiting for experimental features category to get initialized"
  );
  ok(
    categoryHeader.hidden,
    "The category header should be hidden when Home is selected"
  );

  EventUtils.synthesizeMouseAtCenter(experimentalCategory, {}, doc.ownerGlobal);
  await TestUtils.waitForCondition(
    () => !categoryHeader.hidden,
    "Waiting until category is visible"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testSearchFindsExperiments() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let experimentalCategory = doc.getElementById("category-experimental");
  ok(experimentalCategory, "The category exists");
  ok(!experimentalCategory.hidden, "The category is not hidden");

  await TestUtils.waitForCondition(
    () => doc.getElementById("firefoxExperimentalCategory"),
    "Waiting for experimental features category to get initialized"
  );
  await evaluateSearchResults(
    "advanced configuration",
    ["pane-experimental-featureGates"],
    /* include experiments */ true
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
