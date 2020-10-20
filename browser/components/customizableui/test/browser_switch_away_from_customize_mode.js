/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that toggling customize mode doesn't cause side effects
// on the web content browser.

add_task(async function test_resize() {
  // Have the test page set an attribute when a resize happens.
  // We don't expect this to happen anymore after Bug 1671619.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(
    browser,
    `data:text/html;charset=utf-8,
    <script>
      window.addEventListener('resize', () => {
        document.body.setAttribute('was-resized', true);
      });
    </script>
    `
  );

  await BrowserTestUtils.browserLoaded(browser);

  await startCustomizing();
  isnot(browser, gBrowser.selectedBrowser, "Previous browser is not selected");

  await endCustomizing();
  let wasResized = await ContentTask.spawn(browser, {}, async () => {
    return content.document.body.hasAttribute("was-resized");
  });

  is(browser, gBrowser.selectedBrowser, "Previous browser is reselected");
  ok(!wasResized, "Content was not resized after reselecting");
});
