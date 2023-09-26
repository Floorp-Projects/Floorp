/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_receive_punycode_result() {
  let url = "https://www.اختبار.اختبار.org:5000/";

  // eslint-disable-next-line jsdoc/require-jsdoc
  class ResultWithHighlightsProvider extends UrlbarTestUtils.TestProvider {
    startQuery(context, addCallback) {
      let result = Object.assign(
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          ...UrlbarResult.payloadAndSimpleHighlights(context.tokens, {
            url: [url, UrlbarUtils.HIGHLIGHT.TYPED],
          })
        ),
        { suggestedIndex: 0 }
      );
      addCallback(this, result);
    }

    getViewUpdate(result, idsByName) {
      return {};
    }
  }
  let provider = new ResultWithHighlightsProvider();

  registerCleanupFunction(async () => {
    UrlbarProvidersManager.unregisterProvider(provider);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    gURLBar.handleRevert();
  });
  UrlbarProvidersManager.registerProvider(provider);

  info("Open the result popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "org",
    window,
    fireInputEvent: true,
  });
  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  is(row.result.type, UrlbarUtils.RESULT_TYPE.URL, "row.result.type");
  is(
    row.result.payload.displayUrl,
    "اختبار.اختبار.org:5000",
    "Result is trimmed and formatted correctly."
  );
  is(
    row.result.payload.title,
    "www.اختبار.اختبار.org:5000",
    "Result is trimmed and formatted correctly."
  );

  let firstRow = document.querySelector(".urlbarView-row");
  let firstRowUrl = firstRow.querySelector(".urlbarView-url");

  is(
    firstRowUrl.innerHTML.charAt(0),
    "\u200e",
    "UrlbarView row url contains LRM"
  );
  // Tests if highlights are correct after inserting lrm symbol
  is(
    firstRowUrl.querySelector("strong")?.innerText,
    "org",
    "Correct part of url is highlighted"
  );
  is(
    firstRow.querySelector(".urlbarView-title strong")?.innerText,
    "org",
    "Correct part of title is highlighted"
  );
});
