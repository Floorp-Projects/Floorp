/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct shopping-message-bar component appears after requesting analysis for an unanalyzed product.
 */
add_task(async function test_in_progress_analysis_unanalyzed() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_UNANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let unanalyzedProduct = shoppingContainer.unanalyzedProductEl;
          let analysisButton = unanalyzedProduct.analysisButtonEl;

          let messageBarVisiblePromise = ContentTaskUtils.waitForCondition(
            () => {
              return (
                !!shoppingContainer.shoppingMessageBarEl &&
                ContentTaskUtils.is_visible(
                  shoppingContainer.shoppingMessageBarEl
                )
              );
            },
            "Waiting for shopping-message-bar to be visible"
          );

          analysisButton.click();
          await shoppingContainer.updateComplete;

          // Mock the response from analysis status being "pending"
          shoppingContainer.isAnalysisInProgress = true;
          // Add data back, as it was unset as due to the lack of mock APIs.
          // TODO: Support for the mocks will be added in Bug 1853474.
          shoppingContainer.data = Cu.cloneInto(mockData, content);

          await messageBarVisiblePromise;
          await shoppingContainer.updateComplete;

          is(
            shoppingContainer.shoppingMessageBarEl?.getAttribute("type"),
            "analysis-in-progress",
            "shopping-message-bar type should be correct"
          );
        }
      );
    }
  );
});

/**
 * Tests that the correct shopping-message-bar component appears after re-requesting analysis for a stale product,
 * and that component shows progress percentage.
 */
add_task(async function test_in_progress_analysis_stale() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_STALE_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let staleMessageBar = shoppingContainer.shoppingMessageBarEl;
          is(staleMessageBar?.type, "stale", "Got stale message-bar");

          let analysisButton = staleMessageBar.reAnalysisButtonEl;

          let messageBarVisiblePromise = ContentTaskUtils.waitForCondition(
            () => {
              return (
                !!shoppingContainer.shoppingMessageBarEl &&
                ContentTaskUtils.is_visible(
                  shoppingContainer.shoppingMessageBarEl
                )
              );
            },
            "Waiting for shopping-message-bar to be visible"
          );

          analysisButton.click();
          await shoppingContainer.updateComplete;

          // Mock the response from analysis status being "pending"
          shoppingContainer.isAnalysisInProgress = true;
          // Mock the analysis status response with progress.
          shoppingContainer.analysisProgress = 50;
          // Add data back, as it was unset as due to the lack of mock APIs.
          // TODO: Support for the mocks will be added in Bug 1853474.
          shoppingContainer.data = Cu.cloneInto(mockData, content);

          await messageBarVisiblePromise;
          await shoppingContainer.updateComplete;

          let shoppingMessageBarEl = shoppingContainer.shoppingMessageBarEl;
          is(
            shoppingMessageBarEl?.getAttribute("type"),
            "reanalysis-in-progress",
            "shopping-message-bar type should be correct"
          );
          is(
            shoppingMessageBarEl?.getAttribute("progress"),
            "50",
            "shopping-message-bar should have progress"
          );

          let messageBarEl =
            shoppingMessageBarEl?.shadowRoot.querySelector("message-bar");
          is(
            messageBarEl?.getAttribute("style"),
            "--analysis-progress-pcent: 50%;",
            "message-bar should have progress set as a CSS variable"
          );

          let messageBarContainerEl =
            shoppingMessageBarEl?.shadowRoot.querySelector(
              "#message-bar-container"
            );
          is(
            messageBarContainerEl.querySelector("#header")?.dataset.l10nArgs,
            `{"percentage":50}`,
            "message-bar-container header should have progress set as a l10n arg"
          );
        }
      );
    }
  );
});
