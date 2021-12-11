add_task(async function() {
  const childContent =
    "<div style='position: absolute; left: 2200px; background: green; width: 200px; height: 200px;'>" +
    "div</div><div  style='position: absolute; left: 0px; background: red; width: 200px; height: 200px;'>" +
    "<span id='s'>div</span></div>";

  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

  await promiseTabLoadEvent(
    tab,
    "data:text/html;charset=utf-8," + escape(childContent)
  );
  await SimpleTest.promiseFocus(gBrowser.selectedBrowser);

  let remote = gBrowser.selectedBrowser.isRemoteBrowser;

  let findBarOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "findbaropen"
  );
  EventUtils.synthesizeKey("f", { accelKey: true });
  await findBarOpenPromise;

  ok(gFindBarInitialized, "find bar is now initialized");

  // Finds the div in the green box.
  let scrollPromise = remote
    ? BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, "scroll")
    : BrowserTestUtils.waitForEvent(gBrowser, "scroll");
  EventUtils.sendString("div");
  await scrollPromise;

  // Wait for one paint to ensure we've processed the previous key events and scrolling.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    return new Promise(resolve => {
      content.requestAnimationFrame(() => {
        content.setTimeout(resolve, 0);
      });
    });
  });

  // Finds the div in the red box.
  scrollPromise = remote
    ? BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, "scroll")
    : BrowserTestUtils.waitForEvent(gBrowser, "scroll");
  EventUtils.synthesizeKey("g", { accelKey: true });
  await scrollPromise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    Assert.ok(
      content.document.getElementById("s").getBoundingClientRect().left >= 0,
      "scroll should include find result"
    );
  });

  // clear the find bar
  EventUtils.synthesizeKey("a", { accelKey: true });
  EventUtils.synthesizeKey("KEY_Delete");

  gFindBar.close();
  gBrowser.removeCurrentTab();
});
