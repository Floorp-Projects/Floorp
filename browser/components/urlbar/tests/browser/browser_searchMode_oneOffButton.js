/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests one-off search button behavior with search mode.
 */

const TEST_ENGINE = {
  name: "test engine",
  details: {
    alias: "@test",
    template: "http://example.com/?search={searchTerms}",
  },
};

add_task(async function setup() {
  const engine = await Services.search.addEngineWithDetails(
    TEST_ENGINE.name,
    TEST_ENGINE.details
  );

  registerCleanupFunction(async () => {
    await Services.search.removeEngine(engine);
  });
});

add_task(async function test() {
  info("Test no one-off buttons are selected when entering search mode");

  info("Open the result popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Select one of one-off button");
  const oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  ok(oneOffs.selectedButton, "There is a selected one-off button");

  info("Enter search mode");
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: TEST_ENGINE.name,
  });
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE.name,
    entry: "oneoff",
  });
  ok(!oneOffs.selectedButton, "There is no selected one-off button");
});

add_task(async function() {
  info(
    "Test the status of the selected one-off button when exiting search mode with backspace"
  );

  info("Open the result popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Select one of one-off button");
  const oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  ok(oneOffs.selectedButton, "There is a selected one-off button");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs.selectedButton.engine.name,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "oneoff",
    isPreview: true,
  });

  info("Exit from search mode");
  await UrlbarTestUtils.exitSearchMode(window);
  ok(!oneOffs.selectedButton, "There is no any selected one-off button");
});

add_task(async function() {
  info(
    "Test the status of the selected one-off button when exiting search mode with clicking close button"
  );

  info("Open the result popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Select one of one-off button");
  const oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  ok(oneOffs.selectedButton, "There is a selected one-off button");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs.selectedButton.engine.name,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "oneoff",
    isPreview: true,
  });

  info("Exit from search mode");
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  ok(!oneOffs.selectedButton, "There is no any selected one-off button");
});
