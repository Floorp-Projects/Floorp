/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests keyboard selection within UrlbarUtils.RESULT_TYPE.TIP results.

"use strict";

const HELP_URL = "about:mozilla";
const TIP_URL = "about:about";

add_task(async function tipIsSecondResult() {
  let results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        type: "test",
        helpUrl: HELP_URL,
        buttonUrl: TIP_URL,
      }
    ),
  ];

  let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "There should be two results in the view."
  );
  let secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "The second result should be a tip."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    1,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "getSelectedRowIndex should return 1 even though the help button is selected."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    2,
    "The third element should be selected."
  );

  // If this test is running alone, the one-offs will rebuild themselves when
  // the view is opened above, and they may not be visible yet.  Wait for the
  // first one to become visible before trying to select it.
  await TestUtils.waitForCondition(() => {
    return (
      gURLBar.view.oneOffSearchButtons.buttons.firstElementChild &&
      BrowserTestUtils.is_visible(
        gURLBar.view.oneOffSearchButtons.buttons.firstElementChild
      )
    );
  }, "Waiting for first one-off to become visible.");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  await TestUtils.waitForCondition(() => {
    return gURLBar.view.oneOffSearchButtons.selectedButton;
  }, "Waiting for one-off to become selected.");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    -1,
    "No results should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );

  gURLBar.view.close();
  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function tipIsOnlyResult() {
  let results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        type: "test",
        helpUrl:
          "https://support.mozilla.org/en-US/kb/delete-browsing-search-download-history-firefox",
      }
    ),
  ];

  let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "There should be one result in the view."
  );
  let firstResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    firstResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "The first and only result should be a tip."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    1,
    "The second element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    -1,
    "There should be no selection."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );

  gURLBar.view.close();
  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function tipHasNoHelpButton() {
  let results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        type: "test",
      }
    ),
  ];

  let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "There should be two results in the view."
  );
  let secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "The second result should be a tip."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    1,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  await TestUtils.waitForCondition(() => {
    return gURLBar.view.oneOffSearchButtons.selectedButton;
  }, "Waiting for one-off to become selected.");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    -1,
    "No results should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );

  gURLBar.view.close();
  UrlbarProvidersManager.unregisterProvider(provider);
});
