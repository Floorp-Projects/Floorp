/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct shopping-message-bar component appears if a page is not supported.
 * Only footer should be visible.
 */
add_task(async function test_page_not_supported() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingContainer = await getAnalysisDetails(
        browser,
        MOCK_PAGE_NOT_SUPPORTED_RESPONSE
      );

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarType,
        "page-not-supported",
        "shopping-message-bar type should be correct"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);
      verifyFooterVisible(shoppingContainer);
    }
  );
});
