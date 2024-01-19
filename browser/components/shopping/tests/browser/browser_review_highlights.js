/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function verifyHighlights(
  browser,
  data,
  productUrl /* optional, set to override */,
  expectedHighlightTypes,
  expectedLang
) {
  return SpecialPowers.spawn(
    browser,
    [{ data, productUrl, expectedHighlightTypes, expectedLang }],
    async args => {
      let shoppingContainer =
        content.document.querySelector("shopping-container").wrappedJSObject;
      shoppingContainer.data = Cu.cloneInto(args.data, content);
      if (args.productUrl) {
        shoppingContainer.productUrl = args.productUrl;
      }
      await shoppingContainer.updateComplete;

      let reviewHighlights = shoppingContainer.highlightsEl;
      ok(reviewHighlights, "Got review-highlights");
      await reviewHighlights.updateComplete;

      let highlightsList = reviewHighlights.reviewHighlightsListEl;
      await highlightsList.updateComplete;

      is(
        highlightsList.children.length,
        args.expectedHighlightTypes.length,
        "review-highlights should have the right number of highlight-items"
      );

      // Verify number of reviews for each available highlight
      for (let key of args.expectedHighlightTypes) {
        let highlightEl = highlightsList.querySelector(
          `#${content.CSS.escape(key)}`
        );

        ok(highlightEl, "highlight-item for " + key + " exists");
        is(
          highlightEl.lang,
          args.expectedLang,
          `highlight-item should have lang set to ${args.expectedLang}`
        );

        let actualNumberOfReviews = highlightEl.shadowRoot.querySelector(
          ".highlight-details-list"
        ).children.length;
        let expectedNumberOfReviews = Object.values(
          args.data.highlights[key]
        ).flat().length;

        is(
          actualNumberOfReviews,
          expectedNumberOfReviews,
          "There should be equal number of reviews displayed for " + key
        );
      }
    }
  );
}

/**
 * Tests that the review highlights custom components are visible on the page
 * if there is valid data.
 */
add_task(async function test_review_highlights() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let data = MOCK_ANALYZED_PRODUCT_RESPONSE;
      let expectedHighlightTypes = [
        "price",
        "quality",
        "competitiveness",
        "packaging/appearance",
      ];

      info("Testing with default en highlights");
      await verifyHighlights(
        browser,
        data,
        undefined,
        expectedHighlightTypes,
        "en"
      );

      info("Testing with www.amazon.fr");
      await verifyHighlights(
        browser,
        data,
        "https://www.amazon.fr",
        expectedHighlightTypes,
        "fr"
      );

      info("Testing with www.amazon.de");
      await verifyHighlights(
        browser,
        data,
        "https://www.amazon.de",
        expectedHighlightTypes,
        "de"
      );
    }
  );
});

/**
 * Tests that entire highlights components is still hidden if we receive falsy data.
 */
add_task(async function test_review_highlights_no_highlights() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          mockData.highlights = null;

          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let reviewHighlights = shoppingContainer.highlightsEl;
          ok(reviewHighlights, "Got review-highlights");
          await reviewHighlights.updateComplete;

          ok(
            ContentTaskUtils.isHidden(reviewHighlights),
            "review-highlights should not be visible"
          );

          let highlightsList = reviewHighlights?.reviewHighlightsListEl;
          ok(!highlightsList, "review-highlights-list should not be visible");
        }
      );
    }
  );
});

/**
 * Tests that we do not show an invalid highlight type and properly filter data.
 */
add_task(async function test_review_highlights_invalid_type() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      const invalidHighlightData = structuredClone(
        MOCK_ANALYZED_PRODUCT_RESPONSE
      );
      invalidHighlightData.highlights = MOCK_INVALID_KEY_OBJ;
      await SpecialPowers.spawn(
        browser,
        [invalidHighlightData],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let reviewHighlights = shoppingContainer.highlightsEl;
          ok(reviewHighlights, "Got review-highlights");
          await reviewHighlights.updateComplete;

          ok(
            ContentTaskUtils.isHidden(reviewHighlights),
            "review-highlights should not be visible"
          );
        }
      );
    }
  );
});
