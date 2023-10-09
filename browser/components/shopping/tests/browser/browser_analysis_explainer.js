/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the analysis explainer SUMO link is rendered with the expected
 * UTM parameters.
 */
add_task(async function test_analysis_explainer_sumo_link_utm() {
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
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let card =
            shoppingContainer.analysisExplainerEl.shadowRoot.querySelector(
              "shopping-card"
            );

          let href = card.querySelector("a").href;
          let qs = new URL(href).searchParams;
          is(qs.get("as"), "u");
          is(qs.get("utm_source"), "inproduct");
          is(qs.get("utm_campaign"), "learn-more");
          is(qs.get("utm_term"), "core-sidebar");
        }
      );
    }
  );
});
