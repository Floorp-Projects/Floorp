/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_beforeUnload_and_replaceState() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html,<script>window.addEventListener('beforeunload', () => { window.history.replaceState(true, ''); });</script>",
    },
    async function (browser) {
      let initialState = await SpecialPowers.spawn(browser, [], () => {
        return content.history.state;
      });

      is(initialState, null, "history.state should be initially null.");

      let awaitPageShow = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow"
      );
      BrowserCommands.reload();
      await awaitPageShow;

      let updatedState = await SpecialPowers.spawn(browser, [], () => {
        return content.history.state;
      });
      is(updatedState, true, "history.state should have been updated.");
    }
  );
});
