add_task(function*() {
  const childContent = "<div style='position: absolute; left: 2200px; background: green; width: 200px; height: 200px;'>" +
                       "div</div><div  style='position: absolute; left: 0px; background: red; width: 200px; height: 200px;'>" +
                       "<span id='s'>div</span></div>";

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  yield promiseTabLoadEvent(tab, "data:text/html," + escape(childContent));
  yield SimpleTest.promiseFocus(gBrowser.selectedBrowser.contentWindowAsCPOW);

  let findBarOpenPromise = promiseWaitForEvent(gBrowser, "findbaropen");
  EventUtils.synthesizeKey("f", { accelKey: true });
  yield findBarOpenPromise;

  ok(gFindBarInitialized, "find bar is now initialized");

  // Finds the div in the green box.
  let scrollPromise = promiseWaitForEvent(gBrowser, "scroll");
  EventUtils.synthesizeKey("d", {});
  EventUtils.synthesizeKey("i", {});
  EventUtils.synthesizeKey("v", {});
  yield scrollPromise;

  // Finds the div in the red box.
  scrollPromise = promiseWaitForEvent(gBrowser, "scroll");
  EventUtils.synthesizeKey("g", { accelKey: true });
  yield scrollPromise;

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    Assert.ok(content.document.getElementById("s").getBoundingClientRect().left >= 0,
      "scroll should include find result");
  });

  // clear the find bar
  EventUtils.synthesizeKey("a", { accelKey: true });
  EventUtils.synthesizeKey("VK_DELETE", { });

  gFindBar.close();
  gBrowser.removeCurrentTab();
});

