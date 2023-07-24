/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_adjusted_rating() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;
      let rating = MOCK_POPULATED_DATA.adjusted_rating;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_POPULATED_DATA;
      await shoppingContainer.updateComplete;

      let adjustedRating = shoppingContainer.adjustedRatingEl;
      await adjustedRating.updateComplete;

      let mozFiveStar = adjustedRating.ratingEl;
      ok(mozFiveStar, "The moz-five-star element exists");

      is(mozFiveStar.rating, rating, `The moz-five-star rating is ${rating}`);
      is(
        adjustedRating.rating,
        rating,
        `The adjusted rating "rating" is ${rating}`
      );

      rating = 2.55;
      adjustedRating.rating = rating;

      await adjustedRating.updateComplete;

      is(
        mozFiveStar.rating,
        rating,
        `The moz-five-star rating is now ${rating}`
      );
      is(
        adjustedRating.rating,
        rating,
        `The adjusted rating "rating" is now ${rating}`
      );
    }
  );
});
