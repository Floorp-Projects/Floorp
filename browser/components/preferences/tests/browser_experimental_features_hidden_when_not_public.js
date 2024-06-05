/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testNonPublicFeaturesShouldntGetDisplayed() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  const server = new DefinitionServer();
  let definitions = [
    { id: "test-featureA", isPublicJexl: "true", preference: "test.feature.a" },
    {
      id: "test-featureB",
      isPublicJexl: "false",
      preference: "test.feature.b",
    },
    { id: "test-featureC", isPublicJexl: "true", preference: "test.feature.c" },
  ];
  for (let { id, isPublicJexl, preference } of definitions) {
    server.addDefinition({ id, isPublicJexl, preference });
  }
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `about:preferences?definitionsUrl=${encodeURIComponent(
      server.definitionsUrl
    )}#paneExperimental`
  );
  let doc = gBrowser.contentDocument;

  let firstPublicFeatureId = definitions.find(d => d.isPublicJexl == "true").id;
  await TestUtils.waitForCondition(
    () => doc.getElementById(firstPublicFeatureId),
    "wait for the first public feature to get added to the DOM"
  );

  for (let definition of definitions) {
    is(
      !!doc.getElementById(definition.id),
      definition.isPublicJexl == "true",
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
    isPublicJexl: "false",
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
