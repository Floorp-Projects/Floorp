/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testNonPublicFeaturesShouldntGetDisplayed() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  const server = new DefinitionServer();
  let definitions = [
    { id: "test-featureA", isPublic: true, preference: "test.feature.a" },
    { id: "test-featureB", isPublic: false, preference: "test.feature.b" },
    { id: "test-featureC", isPublic: true, preference: "test.feature.c" },
  ];
  for (let { id, isPublic, preference } of definitions) {
    server.addDefinition({ id, isPublic, preference });
  }
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `about:preferences?definitionsUrl=${encodeURIComponent(
      server.definitionsUrl
    )}#paneExperimental`
  );
  let doc = gBrowser.contentDocument;

  await TestUtils.waitForCondition(
    () => doc.getElementById(definitions.find(d => d.isPublic).id),
    "wait for the first public feature to get added to the DOM"
  );

  for (let definition of definitions) {
    is(
      !!doc.getElementById(definition.id),
      definition.isPublic,
      "feature should only be in DOM if it's public: " + definition.id
    );
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testNonPublicFeaturesShouldntGetDisplayed() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.experimental", true],
      ["browser.preferences.experimental.hidden", false],
    ],
  });

  const server = new DefinitionServer();
  server.addDefinition({
    id: "test-hidden",
    isPublic: false,
    preference: "test.feature.hidden",
  });
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `about:preferences?definitionsUrl=${encodeURIComponent(
      server.definitionsUrl
    )}#paneExperimental`
  );
  let doc = gBrowser.contentDocument;

  await TestUtils.waitForCondition(
    () => doc.getElementById("category-experimental").hidden,
    "Wait for Experimental Features section to get hidden"
  );

  ok(
    doc.getElementById("category-experimental").hidden,
    "Experimental Features section should be hidden when all features are hidden"
  );
  ok(
    !doc.getElementById("firefoxExperimentalCategory"),
    "Experimental Features header should not exist when all features are hidden"
  );
  is(
    doc.querySelector(".category[selected]").id,
    "category-general",
    "When the experimental features section is hidden, navigating to #experimental should redirect to #general"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
