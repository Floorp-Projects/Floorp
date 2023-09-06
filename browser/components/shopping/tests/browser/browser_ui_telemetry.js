/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_shopping_reanalysis_event() {
  // testFlushAllChildren() is necessary to deal with the event being
  // recorded in content, but calling testGetValue() in parent.
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickReAnalyzeLink(browser, MOCK_STALE_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();
  var events = Glean.shopping.surfaceReanalyzeClicked.testGetValue();

  Assert.greater(events.length, 0);
  Assert.equal(events[0].category, "shopping");
  Assert.equal(events[0].name, "surface_reanalyze_clicked");
});

function clickReAnalyzeLink(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let shoppingMessageBar = shoppingContainer.shoppingMessageBarEl;
    await shoppingMessageBar.updateComplete;

    await shoppingMessageBar.onClickAnalysisLink();

    return "clicked";
  });
}
