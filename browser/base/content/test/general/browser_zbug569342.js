/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function findBarDisabledOnSomePages() {
  ok(!gFindBar || gFindBar.hidden, "Find bar should not be visible by default");

  let findbarOpenedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.selectedTab,
    "TabFindInitialized"
  );
  document.documentElement.focus();
  // Open the Find bar before we navigate to pages that shouldn't have it.
  EventUtils.synthesizeKey("f", { accelKey: true });
  await findbarOpenedPromise;
  ok(!gFindBar.hidden, "Find bar should be visible");

  let urls = ["about:preferences", "about:addons"];

  for (let url of urls) {
    await testFindDisabled(url);
  }

  // Make sure the find bar is re-enabled after disabled page is closed.
  await testFindEnabled("about:about");
  gFindBar.close();
  ok(gFindBar.hidden, "Find bar should now be hidden");
});

function testFindDisabled(url) {
  return BrowserTestUtils.withNewTab(url, async function (browser) {
    let waitForFindBar = async () => {
      await new Promise(r => requestAnimationFrame(r));
      await new Promise(r => Services.tm.dispatchToMainThread(r));
    };
    ok(
      !gFindBar || gFindBar.hidden,
      "Find bar should not be visible at the start"
    );
    await BrowserTestUtils.synthesizeKey("/", {}, browser);
    await waitForFindBar();
    ok(
      !gFindBar || gFindBar.hidden,
      "Find bar should not be visible after fast find"
    );
    EventUtils.synthesizeKey("f", { accelKey: true });
    await waitForFindBar();
    ok(
      !gFindBar || gFindBar.hidden,
      "Find bar should not be visible after find command"
    );
    ok(
      document.getElementById("cmd_find").getAttribute("disabled"),
      "Find command should be disabled"
    );
  });
}

async function testFindEnabled(url) {
  return BrowserTestUtils.withNewTab(url, async function () {
    ok(
      !document.getElementById("cmd_find").getAttribute("disabled"),
      "Find command should not be disabled"
    );

    // Open Find bar and then close it.
    let findbarOpenedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.selectedTab,
      "TabFindInitialized"
    );
    EventUtils.synthesizeKey("f", { accelKey: true });
    await findbarOpenedPromise;
    ok(!gFindBar.hidden, "Find bar should be visible again");
    EventUtils.synthesizeKey("KEY_Escape");
    ok(gFindBar.hidden, "Find bar should now be hidden");
  });
}
