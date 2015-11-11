/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar works properly

const TEST_URI = TEST_URI_ROOT + "browser_toolbar_basic.html";

add_task(function*() {
  info("Starting browser_toolbar_basic.js");
  yield addTab(TEST_URI);

  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in to start");

  let shown = oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.SHOW);
  document.getElementById("Tools:DevToolbar").doCommand();
  yield shown;
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in checkOpen");

  let close = document.getElementById("developer-toolbar-closebutton");
  ok(close, "Close button exists");

  let toggleToolbox =
    document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
  ok(!isChecked(toggleToolbox), "toggle toolbox button is not checked");

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  ok(isChecked(toggleToolbox), "toggle toolbox button is checked");

  yield addTab("about:blank");
  info("Opened a new tab");

  ok(!isChecked(toggleToolbox), "toggle toolbox button is not checked");

  gBrowser.removeCurrentTab();

  let hidden = oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.HIDE);
  document.getElementById("Tools:DevToolbar").doCommand();
  yield hidden;
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in hidden");

  shown = oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.SHOW);
  document.getElementById("Tools:DevToolbar").doCommand();
  yield shown;
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in after open");

  ok(isChecked(toggleToolbox), "toggle toolbox button is checked");

  hidden = oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.HIDE);
  document.getElementById("developer-toolbar-closebutton").doCommand();
  yield hidden;

  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible after re-close");
});

function isChecked(b) {
  return b.getAttribute("checked") == "true";
}
