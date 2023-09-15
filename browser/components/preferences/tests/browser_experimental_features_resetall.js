/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// It doesn't matter what two preferences are used here, as long as the first is a built-in
// one that defaults to false and the second defaults to true.
const KNOWN_PREF_1 = "browser.display.use_system_colors";
const KNOWN_PREF_2 = "browser.autofocus";

// This test verifies that pressing the reset all button for experimental features
// resets all of the checkboxes to their default state.
add_task(async function testResetAll() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.experimental", true],
      ["test.featureA", false],
      ["test.featureB", true],
      [KNOWN_PREF_1, false],
      [KNOWN_PREF_2, true],
    ],
  });

  // Add a number of test features.
  const server = new DefinitionServer();
  let definitions = [
    {
      id: "test-featureA",
      preference: "test.featureA",
      defaultValue: false,
    },
    {
      id: "test-featureB",
      preference: "test.featureB",
      defaultValue: true,
    },
    {
      id: "test-featureC",
      preference: KNOWN_PREF_1,
      defaultValue: false,
    },
    {
      id: "test-featureD",
      preference: KNOWN_PREF_2,
      defaultValue: true,
    },
  ];
  for (let { id, preference, defaultValue } of definitions) {
    server.addDefinition({ id, preference, defaultValue, isPublic: true });
  }

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `about:preferences?definitionsUrl=${encodeURIComponent(
      server.definitionsUrl
    )}#paneExperimental`
  );
  let doc = gBrowser.contentDocument;

  await TestUtils.waitForCondition(
    () => doc.getElementById(definitions[definitions.length - 1].id),
    "wait for the first public feature to get added to the DOM"
  );

  // Check the initial state of each feature.
  ok(!Services.prefs.getBoolPref("test.featureA"), "initial state A");
  ok(Services.prefs.getBoolPref("test.featureB"), "initial state B");
  ok(!Services.prefs.getBoolPref(KNOWN_PREF_1), "initial state C");
  ok(Services.prefs.getBoolPref(KNOWN_PREF_2), "initial state D");

  // Modify the state of some of the features.
  doc.getElementById("test-featureC").click();
  doc.getElementById("test-featureD").click();
  ok(!Services.prefs.getBoolPref("test.featureA"), "modified state A");
  ok(Services.prefs.getBoolPref("test.featureB"), "modified state B");
  ok(Services.prefs.getBoolPref(KNOWN_PREF_1), "modified state C");
  ok(!Services.prefs.getBoolPref(KNOWN_PREF_2), "modified state D");

  // State after reset.
  let prefChangedPromise = new Promise(resolve => {
    Services.prefs.addObserver(KNOWN_PREF_2, function observer() {
      Services.prefs.removeObserver(KNOWN_PREF_2, observer);
      resolve();
    });
  });
  doc.getElementById("experimentalCategory-reset").click();
  await prefChangedPromise;

  // The preferences will be reset to the default value for the feature.
  ok(!Services.prefs.getBoolPref("test.featureA"), "after reset state A");
  ok(Services.prefs.getBoolPref("test.featureB"), "after reset state B");
  ok(!Services.prefs.getBoolPref(KNOWN_PREF_1), "after reset state C");
  ok(Services.prefs.getBoolPref(KNOWN_PREF_2), "after reset state D");
  ok(
    !doc.getElementById("test-featureA").checked,
    "after reset checkbox state A"
  );
  ok(
    doc.getElementById("test-featureB").checked,
    "after reset checkbox state B"
  );
  ok(
    !doc.getElementById("test-featureC").checked,
    "after reset checkbox state C"
  );
  ok(
    doc.getElementById("test-featureD").checked,
    "after reset checkbox state D"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
