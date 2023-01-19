/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test() {
  let results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/1" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/2" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        helpUrl: "http://example.com/",
        type: "test",
        titleL10n: { id: "urlbar-search-tips-confirm" },
        buttons: [
          {
            url: "http://example.com/",
            l10n: { id: "urlbar-search-tips-confirm" },
          },
        ],
      }
    ),
  ];

  results[0].heuristic = true;

  let selectionCount = 0;
  let provider = new UrlbarTestUtils.TestProvider({
    results,
    priority: 1,
    onSelection: (result, element) => {
      selectionCount++;
    },
  });
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);

  while (!oneOffs.selectedButton) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }

  Assert.equal(selectionCount, 4, "We selected the four elements in the view.");
  UrlbarProvidersManager.unregisterProvider(provider);
});
