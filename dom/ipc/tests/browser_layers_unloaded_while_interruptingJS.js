/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_check_layers_cleared() {
  let initialTab = gBrowser.selectedTab;
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    await ContentTask.spawn(browser, null, () => {
      return new Promise(resolve => {
        content.requestAnimationFrame(() => {
          content.setTimeout(
            "let start = performance.now(); while (performance.now() < start + 5000);"
          );
          resolve();
        });
      });
    });
    let layersCleared = BrowserTestUtils.waitForEvent(
      window,
      "MozLayerTreeCleared"
    );
    let startWaiting = performance.now();
    await BrowserTestUtils.switchTab(gBrowser, initialTab);
    await layersCleared;
    ok(
      performance.now() < startWaiting + 2000,
      "MozLayerTreeCleared should be dispatched while the script is still running"
    );
  });
});
