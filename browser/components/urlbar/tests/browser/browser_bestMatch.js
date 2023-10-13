/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests best match rows in the view.

"use strict";

// Tests a non-sponsored best match row.
add_task(async function nonsponsored() {
  let result = makeBestMatchResult();
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests a non-sponsored best match row with a help button.
add_task(async function nonsponsoredHelpButton() {
  let result = makeBestMatchResult({ helpUrl: "https://example.com/help" });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result, hasHelpUrl: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests a sponsored best match row.
add_task(async function sponsored() {
  let result = makeBestMatchResult({ isSponsored: true });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result, isSponsored: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests a sponsored best match row with a help button.
add_task(async function sponsoredHelpButton() {
  let result = makeBestMatchResult({
    isSponsored: true,
    helpUrl: "https://example.com/help",
  });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({ result, isSponsored: true, hasHelpUrl: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests keyboard selection.
add_task(async function keySelection() {
  let result = makeBestMatchResult({
    isSponsored: true,
    helpUrl: "https://example.com/help",
  });

  await withProvider(result, async () => {
    // Ordered list of class names of the elements that should be selected.
    let expectedClassNames = ["urlbarView-row-inner", "urlbarView-button-menu"];

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({
      result,
      isSponsored: true,
      hasHelpUrl: true,
    });

    // Test with the tab key in order vs. reverse order.
    for (let reverse of [false, true]) {
      info("Doing TAB key selection: " + JSON.stringify({ reverse }));

      let classNames = [...expectedClassNames];
      if (reverse) {
        classNames.reverse();
      }

      let sendKey = () => {
        EventUtils.synthesizeKey("KEY_Tab", { shiftKey: reverse });
      };

      // Move selection through each expected element.
      for (let className of classNames) {
        info("Expecting selection: " + className);
        sendKey();
        Assert.ok(gURLBar.view.isOpen, "View remains open");
        let { selectedElement } = gURLBar.view;
        Assert.ok(selectedElement, "Selected element exists");
        Assert.ok(
          selectedElement.classList.contains(className),
          "Expected element is selected"
        );
      }
      sendKey();
      Assert.ok(
        gURLBar.view.isOpen,
        "View remains open after keying through best match row"
      );
    }

    await UrlbarTestUtils.promisePopupClose(window);
  });
});

async function checkBestMatchRow({
  result,
  isSponsored = false,
  hasHelpUrl = false,
}) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "One result is present"
  );

  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let { row } = details.element;

  let favicon = row._elements.get("favicon");
  Assert.ok(favicon, "Row has a favicon");

  let title = row._elements.get("title");
  Assert.ok(title, "Row has a title");
  Assert.ok(title.textContent, "Row title has non-empty textContext");
  Assert.equal(title.textContent, result.payload.title, "Row title is correct");

  let url = row._elements.get("url");
  Assert.ok(url, "Row has a URL");
  Assert.ok(url.textContent, "Row URL has non-empty textContext");
  Assert.equal(
    url.textContent,
    result.payload.displayUrl,
    "Row URL is correct"
  );

  let button = row._buttons.get("menu");
  Assert.equal(
    !!result.payload.helpUrl,
    hasHelpUrl,
    "Sanity check: Row's expected hasHelpUrl matches result"
  );
  if (hasHelpUrl) {
    Assert.ok(button, "Row with helpUrl has a help or menu button");
  } else {
    Assert.ok(
      !button,
      "Row without helpUrl does not have a help or menu button"
    );
  }
}

async function withProvider(result, callback) {
  let provider = new UrlbarTestUtils.TestProvider({
    results: [result],
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(provider);
  try {
    await callback();
  } finally {
    UrlbarProvidersManager.unregisterProvider(provider);
  }
}

function makeBestMatchResult(payloadExtra = {}) {
  return Object.assign(
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights([], {
        title: "Test best match",
        url: "https://example.com/best-match",
        ...payloadExtra,
      })
    ),
    { isBestMatch: true }
  );
}
