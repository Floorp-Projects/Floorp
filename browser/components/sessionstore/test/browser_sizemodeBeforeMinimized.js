add_task(async function test() {
  // Test for bugfix 384278. Confirms that sizemodeBeforeMinimized is set properly when window state is saved.
  const win = await BrowserTestUtils.openNewBrowserWindow();

  async function changeSizeMode(mode) {
    let promise = BrowserTestUtils.waitForEvent(win, "sizemodechange");
    win[mode]();
    await promise;
  }

  function checkCurrentState(sizemodeBeforeMinimized) {
    let state = JSON.parse(ss.getWindowState(win));
    let winState = state.windows[0];
    is(winState.sizemodeBeforeMinimized, sizemodeBeforeMinimized, "sizemodeBeforeMinimized should match");
  }

  if (win.windowState != win.STATE_NORMAL) {
    await changeSizeMode("restore");
  }
  await forceSaveState();
  await changeSizeMode("minimize");
  checkCurrentState("normal");

  await changeSizeMode("maximize");
  await forceSaveState();
  await changeSizeMode("minimize");
  checkCurrentState("maximized");

  // Clean up.
  await BrowserTestUtils.closeWindow(win);
});
