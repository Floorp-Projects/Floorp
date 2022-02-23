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
    await checkBestMatchRow({ result, hasHelpButton: true });
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
    await checkBestMatchRow({ result, isSponsored: true, hasHelpButton: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Tests keyboard selection using the arrow keys.
add_task(async function arrowKeys() {
  await doKeySelectionTest(false);
});

// Tests keyboard selection using the tab key.
add_task(async function tabKey() {
  await doKeySelectionTest(true);
});

async function doKeySelectionTest(useTabKey) {
  info(`Starting key selection test with useTabKey=${useTabKey}`);

  let result = makeBestMatchResult({
    isSponsored: true,
    helpUrl: "https://example.com/help",
  });

  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });

    await checkBestMatchRow({ result, isSponsored: true, hasHelpButton: true });

    // Ordered list of class names of the elements that should be selected as
    // the tab key is pressed.
    let expectedClassNames = [
      "urlbarView-row-inner",
      "urlbarView-button-block",
      "urlbarView-button-help",
    ];

    let sendKey = reverse => {
      if (useTabKey) {
        EventUtils.synthesizeKey("KEY_Tab", { shiftKey: reverse });
      } else if (reverse) {
        EventUtils.synthesizeKey("KEY_ArrowUp");
      } else {
        EventUtils.synthesizeKey("KEY_ArrowDown");
      }
    };

    // First tab in forward order and then in reverse order.
    for (let reverse of [false, true]) {
      info(`Doing key selection with reverse=${reverse}`);

      let classNames = [...expectedClassNames];
      if (reverse) {
        classNames.reverse();
      }

      for (let className of classNames) {
        sendKey(reverse);
        Assert.ok(gURLBar.view.isOpen, "View remains open");
        let { selectedElement } = gURLBar.view;
        Assert.ok(selectedElement, "Selected element exists");
        Assert.ok(
          selectedElement.classList.contains(className),
          "Expected element is selected"
        );
      }
      sendKey(reverse);
      Assert.ok(
        gURLBar.view.isOpen,
        "View remains open after keying through best match row"
      );
    }

    await UrlbarTestUtils.promisePopupClose(window);
  });
}

async function checkBestMatchRow({
  result,
  isSponsored = false,
  hasHelpButton = false,
}) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "One result is present"
  );

  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let { row } = details.element;

  Assert.equal(row.getAttribute("type"), "bestmatch", "row[type] is bestmatch");

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

  let bottom = row._elements.get("bottom");
  Assert.ok(bottom, "Row has a bottom");
  Assert.equal(
    !!result.payload.isSponsored,
    isSponsored,
    "Sanity check: Row's expected isSponsored matches result's"
  );
  if (isSponsored) {
    Assert.equal(
      bottom.textContent,
      "Sponsored",
      "Sponsored row bottom has Sponsored textContext"
    );
  } else {
    Assert.equal(
      bottom.textContent,
      "",
      "Non-sponsored row bottom has empty textContext"
    );
  }

  let blockButton = row._buttons.get("block");
  Assert.ok(blockButton, "Row has a block button");

  let helpButton = row._buttons.get("help");
  Assert.equal(
    !!result.payload.helpUrl,
    hasHelpButton,
    "Sanity check: Row's expected hasHelpButton matches result"
  );
  if (hasHelpButton) {
    Assert.ok(helpButton, "Row with helpUrl has a helpButton");
  } else {
    Assert.ok(!helpButton, "Row without helpUrl does not have a helpButton");
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
