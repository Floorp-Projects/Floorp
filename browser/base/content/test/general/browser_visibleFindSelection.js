
function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    ok(true, "Load listener called");
    waitForFocus(onFocus, content);
  }, true);

  content.location = "data:text/html,<div style='position: absolute; left: 2200px; background: green; width: 200px; height: 200px;'>div</div><div style='position: absolute; left: 0px; background: red; width: 200px; height: 200px;'><span id='s'>div</span></div>";
}

function onFocus() {
  EventUtils.synthesizeKey("f", { accelKey: true });
  ok(gFindBarInitialized, "find bar is now initialized");

  EventUtils.synthesizeKey("d", {});
  EventUtils.synthesizeKey("i", {});
  EventUtils.synthesizeKey("v", {});
  // finds the div in the green box

  EventUtils.synthesizeKey("g", { accelKey: true });
  // finds the div in the red box

  var rect = content.document.getElementById("s").getBoundingClientRect();
  ok(rect.left >= 0, "scroll should include find result");

  // clear the find bar
  EventUtils.synthesizeKey("a", { accelKey: true });
  EventUtils.synthesizeKey("VK_DELETE", { });

  gFindBar.close();
  gBrowser.removeCurrentTab();
  finish();
}
