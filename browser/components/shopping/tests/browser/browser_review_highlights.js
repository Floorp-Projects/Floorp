/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the review highlights custom components are visible on the page
 * if there is valid data.
 */
add_task(async function test_review_highlights() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;
      const EXPECTED_KEYS = ["price", "quality", "competitiveness"];

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_POPULATED_DATA;
      await shoppingContainer.updateComplete;

      let reviewHighlights = shoppingContainer.highlightsEl;
      ok(reviewHighlights, "Got review-highlights");
      await reviewHighlights.updateComplete;

      let highlightsList = reviewHighlights.reviewHighlightsListEl;
      await highlightsList.updateComplete;

      is(
        highlightsList.children.length,
        EXPECTED_KEYS.length,
        "review-highlights should have the right number of highlight-items"
      );

      // Verify number of reviews for each available highlight
      for (let key of EXPECTED_KEYS) {
        let highlightEl = highlightsList.querySelector(`#${key}`);
        ok(highlightEl, "highlight-item for " + key + " exists");

        let actualNumberOfReviews = highlightEl.shadowRoot.querySelector(
          ".highlight-details-list"
        ).children.length;
        let expectedNumberOfReviews = Object.values(
          MOCK_POPULATED_DATA.highlights[key]
        ).flat().length;
        is(
          actualNumberOfReviews,
          expectedNumberOfReviews,
          "There should be equal number of reviews displayed for " + key
        );
      }
    }
  );
});

/**
 * Tests that entire highlights components is still hidden if we receive falsy data.
 */
add_task(async function test_review_highlights_no_highlights() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;
      const noHighlightData = MOCK_POPULATED_DATA;
      noHighlightData.highlights = null;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = noHighlightData;
      await shoppingContainer.updateComplete;

      let reviewHighlights = shoppingContainer.highlightsEl;
      ok(reviewHighlights, "Got review-highlights");
      await reviewHighlights.updateComplete;

      ok(
        BrowserTestUtils.is_hidden(reviewHighlights),
        "review-highlights should not be visible"
      );

      let highlightsList = reviewHighlights?.reviewHighlightsListEl;
      ok(!highlightsList, "review-highlights-list should not be visible");
    }
  );
});

/**
 * Tests that we do not show an invalid highlight type and properly filter data.
 */
add_task(async function test_review_highlights_invalid_type() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;
      const invalidHighlightData = MOCK_POPULATED_DATA;
      invalidHighlightData.highlights = MOCK_INVALID_KEY_OBJ;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = invalidHighlightData;
      await shoppingContainer.updateComplete;

      let reviewHighlights = shoppingContainer.highlightsEl;
      ok(reviewHighlights, "Got review-highlights");
      await reviewHighlights.updateComplete;

      ok(
        BrowserTestUtils.is_hidden(reviewHighlights),
        "review-highlights should not be visible"
      );
    }
  );
});
