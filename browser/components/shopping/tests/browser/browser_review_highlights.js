/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ShoppingUtils } = ChromeUtils.importESModule(
  "chrome://browser/content/shopping/ShoppingUtils.sys.mjs"
);

/**
 * Tests that the review highlights custom components are visible on the page
 * if there is valid data.
 */
add_task(async function test_review_highlights() {
  let sandbox = sinon.createSandbox();
  const MOCK_OBJ = MOCK_POPULATED_OBJ;
  sandbox.stub(ShoppingUtils, "getHighlights").returns(MOCK_OBJ);

  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;
      const EXPECTED_KEYS = ["price", "quality", "competitiveness"];

      let reviewHighlights = document.querySelector("review-highlights");
      ok(reviewHighlights, "review-highlights should be visible");

      let highlightsList = reviewHighlights.reviewHighlightsListEl;

      await BrowserTestUtils.waitForMutationCondition(
        highlightsList,
        { childList: true },
        () => highlightsList.querySelector("highlight-item")
      );

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
        let expectedNumberOfReviews = Object.values(MOCK_OBJ[key]).flat()
          .length;
        is(
          actualNumberOfReviews,
          expectedNumberOfReviews,
          "There should be equal number of reviews displayed for " + key
        );
      }

      sandbox.restore();
    }
  );
});

/**
 * Tests that entire highlights components is still hidden if we receive falsy data.
 */
add_task(async function test_review_highlights_no_highlights() {
  let sandbox = sinon.createSandbox();
  const MOCK_OBJ = null;
  sandbox.stub(ShoppingUtils, "getHighlights").returns(MOCK_OBJ);

  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let reviewHighlights = document.querySelector("review-highlights");
      ok(
        BrowserTestUtils.is_hidden(reviewHighlights),
        "review-highlights should not be visible"
      );

      let highlightsList = reviewHighlights?.reviewHighlightsListEl;

      ok(!highlightsList, "review-highlights-list should not be visible");
      sandbox.restore();
    }
  );
});

/**
 * Tests that the entire highlights component is still hidden if an error occurs when fetching highlights.
 */
add_task(async function test_review_highlights_error() {
  let sandbox = sinon.createSandbox();
  const MOCK_OBJ = MOCK_ERROR_OBJ;
  sandbox.stub(ShoppingUtils, "getHighlights").returns(MOCK_OBJ);

  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let reviewHighlights = document.querySelector("review-highlights");
      ok(
        BrowserTestUtils.is_hidden(reviewHighlights),
        "review-highlights should not be visible"
      );

      let highlightsList = reviewHighlights?.reviewHighlightsListEl;

      ok(!highlightsList, "review-highlights-list should not be visible");
      sandbox.restore();
    }
  );
});

/**
 * Tests that we do not show an invalid highlight type and properly filter data.
 */
add_task(async function test_review_highlights_invalid_type() {
  let sandbox = sinon.createSandbox();
  // This mock object only has invalid data, so we should expect an empty object after filtering.
  const MOCK_OBJ = MOCK_INVALID_KEY_OBJ;
  sandbox.stub(ShoppingUtils, "getHighlights").returns(MOCK_OBJ);

  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let reviewHighlights = document.querySelector("review-highlights");
      ok(
        BrowserTestUtils.is_hidden(reviewHighlights),
        "review-highlights should not be visible"
      );

      sandbox.restore();
    }
  );
});
