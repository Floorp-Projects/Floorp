/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function testSidebarKeyToggle(key, options, expectedSidebarId) {
  EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {});
  let promiseShown = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  EventUtils.synthesizeKey(key, options);
  await promiseShown;
  Assert.equal(
    document.getElementById("sidebar-box").getAttribute("sidebarcommand"),
    expectedSidebarId
  );
  EventUtils.synthesizeKey(key, options);
  Assert.ok(!SidebarUI.isOpen);
}

add_task(async function test_sidebar_keys() {
  registerCleanupFunction(() => SidebarUI.hide());

  await testSidebarKeyToggle("b", { accelKey: true }, "viewBookmarksSidebar");
  if (AppConstants.platform == "win") {
    await testSidebarKeyToggle("i", { accelKey: true }, "viewBookmarksSidebar");
  }

  let options = { accelKey: true, shiftKey: AppConstants.platform == "macosx" };
  await testSidebarKeyToggle("h", options, "viewHistorySidebar");
});
