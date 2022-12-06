/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests best match rows in the view. See also:
//
// browser_quicksuggest_bestMatch.js
//   UI test for quick suggest best matches specifically
// test_quicksuggest_bestMatch.js
//   Tests triggering quick suggest best matches and things that don't depend on
//   the view

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

// Tests a non-sponsored best match row with help and block buttons.
add_task(async function nonsponsoredHelpAndBlockButtons() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.blockingEnabled", true]],
  });
  let result = makeBestMatchResult({ helpUrl: "https://example.com/help" });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({
      result,
      hasHelpButton: true,
      hasBlockButton: true,
    });
    await UrlbarTestUtils.promisePopupClose(window);
  });
  await SpecialPowers.popPrefEnv();
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

// Tests a sponsored best match row with help and block buttons.
add_task(async function sponsoredHelpAndBlockButtons() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.blockingEnabled", true]],
  });
  let result = makeBestMatchResult({
    isSponsored: true,
    helpUrl: "https://example.com/help",
  });
  await withProvider(result, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkBestMatchRow({
      result,
      isSponsored: true,
      hasHelpButton: true,
      hasBlockButton: true,
    });
    await UrlbarTestUtils.promisePopupClose(window);
  });
  await SpecialPowers.popPrefEnv();
});

// Tests keyboard selection.
add_task(async function keySelection() {
  let result = makeBestMatchResult({
    isSponsored: true,
    helpUrl: "https://example.com/help",
  });

  await withProvider(result, async () => {
    // Ordered list of class names of the elements that should be selected.
    let expectedClassNames = [
      "urlbarView-row-inner",
      "urlbarView-button-block",
      "urlbarView-button-help",
    ];

    // Test with and without the block button.
    for (let showBlockButton of [false, true]) {
      UrlbarPrefs.set("bestMatch.blockingEnabled", showBlockButton);

      // The block button is not immediately removed or added when
      // `bestMatch.blockingEnabled` is toggled while the panel is open, so we
      // need to do a new search each time we change it.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "test",
      });
      await checkBestMatchRow({
        result,
        isSponsored: true,
        hasHelpButton: true,
        hasBlockButton: showBlockButton,
      });

      // Test with the tab key vs. arrow keys and in order vs. reverse order.
      for (let useTabKey of [false, true]) {
        for (let reverse of [false, true]) {
          info(
            "Doing key selection: " +
              JSON.stringify({ showBlockButton, useTabKey, reverse })
          );

          let classNames = [...expectedClassNames];
          if (!showBlockButton) {
            classNames.splice(classNames.indexOf("urlbarView-button-block"), 1);
          }
          if (reverse) {
            classNames.reverse();
          }

          let sendKey = () => {
            if (useTabKey) {
              EventUtils.synthesizeKey("KEY_Tab", { shiftKey: reverse });
            } else if (reverse) {
              EventUtils.synthesizeKey("KEY_ArrowUp");
            } else {
              EventUtils.synthesizeKey("KEY_ArrowDown");
            }
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
      }

      await UrlbarTestUtils.promisePopupClose(window);
      UrlbarPrefs.clear("bestMatch.blockingEnabled");
    }
  });
});

async function checkBestMatchRow({
  result,
  isSponsored = false,
  hasHelpButton = false,
  hasBlockButton = false,
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
  if (hasBlockButton) {
    Assert.ok(blockButton, "Row has a block button");
  } else {
    Assert.ok(!blockButton, "Row does not have a block button");
  }

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
