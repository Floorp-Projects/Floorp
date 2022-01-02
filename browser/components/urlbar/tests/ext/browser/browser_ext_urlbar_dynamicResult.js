/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests dynamic results using the WebExtension Experiment API.

"use strict";

add_task(async function test() {
  let ext = await loadExtension({
    extraFiles: {
      "dynamicResult.css": await (
        await fetch("file://" + getTestFilePath("dynamicResult.css"))
      ).text(),
    },
    background: async () => {
      browser.experiments.urlbar.addDynamicResultType("testDynamicType");
      browser.experiments.urlbar.addDynamicViewTemplate("testDynamicType", {
        stylesheet: "dynamicResult.css",
        children: [
          {
            name: "text",
            tag: "span",
          },
          {
            name: "button",
            tag: "span",
            attributes: {
              role: "button",
            },
          },
        ],
      });
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "restricting";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        return [
          {
            type: "dynamic",
            source: "local",
            heuristic: true,
            payload: {
              dynamicType: "testDynamicType",
            },
          },
        ];
      }, "test");
      browser.experiments.urlbar.onViewUpdateRequested.addListener(payload => {
        return {
          text: {
            textContent: "This is a dynamic result.",
          },
          button: {
            textContent: "Click Me",
          },
        };
      }, "test");
      browser.urlbar.onResultPicked.addListener((payload, elementName) => {
        browser.test.sendMessage("onResultPicked", [payload, elementName]);
      }, "test");
    },
  });

  // Wait for the provider and dynamic type to be registered before continuing.
  await TestUtils.waitForCondition(
    () =>
      UrlbarProvidersManager.getProvider("test") &&
      UrlbarResult.getDynamicResultType("testDynamicType"),
    "Waiting for provider and dynamic type to be registered"
  );
  Assert.ok(
    UrlbarProvidersManager.getProvider("test"),
    "Provider should be registered"
  );
  Assert.ok(
    UrlbarResult.getDynamicResultType("testDynamicType"),
    "Dynamic type should be registered"
  );

  // Do a search.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
    waitForFocus: SimpleTest.waitForFocus,
  });

  // Get the row.
  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  Assert.equal(
    row.result.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "row.result.type"
  );
  Assert.equal(
    row.getAttribute("dynamicType"),
    "testDynamicType",
    "row[dynamicType]"
  );

  let text = row.querySelector(".urlbarView-dynamic-testDynamicType-text");

  // The view's call to provider.getViewUpdate is async, so we need to make sure
  // the update has been applied before continuing to avoid intermittent
  // failures.
  await TestUtils.waitForCondition(
    () => text.textContent == "This is a dynamic result."
  );

  // Check the elements.
  Assert.equal(
    text.textContent,
    "This is a dynamic result.",
    "text.textContent"
  );
  let button = row.querySelector(".urlbarView-dynamic-testDynamicType-button");
  Assert.equal(button.textContent, "Click Me", "button.textContent");

  // The result's button should be selected since the result is the heuristic.
  Assert.equal(
    UrlbarTestUtils.getSelectedElement(window),
    button,
    "Button should be selected"
  );

  // Pick the button.
  let pickPromise = ext.awaitMessage("onResultPicked");
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Enter")
  );
  let [payload, elementName] = await pickPromise;
  Assert.equal(payload.dynamicType, "testDynamicType", "Picked payload");
  Assert.equal(elementName, "button", "Picked element name");

  await ext.unload();
});
