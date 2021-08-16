/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test verifies that searching filters the features to just that subset that
// contains the search terms.
add_task(async function testFilterFeatures() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  // Add a number of test features.
  const server = new DefinitionServer();
  let definitions = [
    {
      id: "test-featureA",
      preference: "test.featureA",
      title: "Experimental Feature 1",
      description: "This is a fun experimental feature you can enable",
      result: true,
    },
    {
      id: "test-featureB",
      preference: "test.featureB",
      title: "Experimental Thing 2",
      description: "This is a very boring experimental tool",
      result: false,
    },
    {
      id: "test-featureC",
      preference: "test.featureC",
      title: "Experimental Thing 3",
      description: "This is a fun experimental feature for you can enable",
      result: true,
    },
    {
      id: "test-featureD",
      preference: "test.featureD",
      title: "Experimental Thing 4",
      description: "This is a not a checkbox that you should be enabling",
      result: false,
    },
  ];
  for (let { id, preference } of definitions) {
    server.addDefinition({ id, preference, isPublic: true });
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

  // Manually modify the labels of the features that were just added, so that the test
  // can rely on consistent search terms.
  for (let definition of definitions) {
    let mainItem = doc.getElementById(definition.id);
    mainItem.label = definition.title;
    mainItem.removeAttribute("data-l10n-id");
    let descItem = doc.getElementById(definition.id + "-description");
    descItem.textContent = definition.description;
    descItem.removeAttribute("data-l10n-id");
  }

  // First, check that all of the items are visible by default.
  for (let definition of definitions) {
    checkVisibility(
      doc.getElementById(definition.id),
      true,
      `${definition.id} should be initially visible`
    );
  }

  // After searching, only a subset should be visible.
  await enterSearch(doc, "feature");

  for (let definition of definitions) {
    checkVisibility(
      doc.getElementById(definition.id),
      definition.result,
      `${definition.id} should be ${
        definition.result ? "visible" : "hidden"
      } after first search`
    );
    info("Text for item was: " + doc.getElementById(definition.id).textContent);
  }

  // Further restrict the search to only a single item.
  await enterSearch(doc, " you");

  let shouldBeVisible = true;
  for (let definition of definitions) {
    checkVisibility(
      doc.getElementById(definition.id),
      shouldBeVisible,
      `${definition.id} should be ${
        shouldBeVisible ? "visible" : "hidden"
      } after further search`
    );
    shouldBeVisible = false;
  }

  // Reset the search entirely.
  let searchInput = doc.getElementById("searchInput");
  searchInput.value = "";
  searchInput.doCommand();

  // Clearing the search will go to the general pane so switch back to the experimental pane.
  EventUtils.synthesizeMouseAtCenter(
    doc.getElementById("category-experimental"),
    {},
    gBrowser.contentWindow
  );

  for (let definition of definitions) {
    checkVisibility(
      doc.getElementById(definition.id),
      true,
      `${definition.id} should be visible after search cleared`
    );
  }

  // Simulate entering a search and then clicking one of the category labels. The search
  // should reset each time.
  for (let category of ["category-search", "category-experimental"]) {
    await enterSearch(doc, "feature");

    for (let definition of definitions) {
      checkVisibility(
        doc.getElementById(definition.id),
        definition.result,
        `${definition.id} should be ${
          definition.result ? "visible" : "hidden"
        } after next search`
      );
    }

    EventUtils.synthesizeMouseAtCenter(
      doc.getElementById(category),
      {},
      gBrowser.contentWindow
    );

    for (let definition of definitions) {
      checkVisibility(
        doc.getElementById(definition.id),
        true,
        `${definition.id} should be visible after category change to ${category}`
      );
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function checkVisibility(element, expected, desc) {
  return expected
    ? is_element_visible(element, desc)
    : is_element_hidden(element, desc);
}

function enterSearch(doc, query) {
  let searchInput = doc.getElementById("searchInput");
  searchInput.focus();

  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow,
    "PreferencesSearchCompleted",
    evt => evt.detail == query
  );

  EventUtils.sendString(query);

  return searchCompletedPromise;
}
