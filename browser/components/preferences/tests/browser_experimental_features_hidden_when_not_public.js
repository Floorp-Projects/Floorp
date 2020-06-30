/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://featuregates/FeatureGate.jsm", this);
ChromeUtils.import(
  "resource://featuregates/FeatureGateImplementation.jsm",
  this
);
ChromeUtils.import("resource://testing-common/httpd.js", this);

class DefinitionServer {
  constructor(definitionOverrides = []) {
    this.server = new HttpServer();
    this.server.registerPathHandler("/definitions.json", this);
    this.definitions = {};

    for (const override of definitionOverrides) {
      this.addDefinition(override);
    }

    this.server.start();
    registerCleanupFunction(
      () => new Promise(resolve => this.server.stop(resolve))
    );
  }

  // for nsIHttpRequestHandler
  handle(request, response) {
    response.write(JSON.stringify(this.definitions));
  }

  get definitionsUrl() {
    const { primaryScheme, primaryHost, primaryPort } = this.server.identity;
    return `${primaryScheme}://${primaryHost}:${primaryPort}/definitions.json`;
  }

  addDefinition(overrides = {}) {
    const definition = {
      id: "test-feature",
      // These l10n IDs are just random so we have some text to display
      title: "pane-experimental-subtitle",
      description: "pane-experimental-description",
      restartRequired: false,
      type: "boolean",
      preference: "test.feature",
      defaultValue: false,
      isPublic: false,
      ...overrides,
    };
    // convert targeted values, used by fromId
    definition.isPublic = { default: definition.isPublic };
    definition.defaultValue = { default: definition.defaultValue };
    this.definitions[definition.id] = definition;
    return definition;
  }
}

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
