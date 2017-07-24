/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the developer toolbar works properly

const {gDevToolsBrowser} = require("devtools/client/framework/devtools-browser");

const TEST_URI = TEST_URI_ROOT + "doc_toolbar_basic.html";

add_task(function* () {
  info("Starting browser_toolbar_basic.js");
  yield addTab(TEST_URI);

  let toolbar = gDevToolsBrowser.getDeveloperToolbar(window);
  ok(!toolbar.visible, "DeveloperToolbar is not visible in to start");

  let shown = oneTimeObserve(toolbar.NOTIFICATIONS.SHOW);
  document.getElementById("menu_devToolbar").doCommand();
  yield shown;
  ok(toolbar.visible, "DeveloperToolbar is visible in checkOpen");

  let close = document.getElementById("developer-toolbar-closebutton");
  ok(close, "Close button exists");

  let toggleToolbox =
    document.getElementById("menu_devToolbox");
  ok(!isChecked(toggleToolbox), "toggle toolbox button is not checked");

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.showToolbox(target, "inspector");
  ok(isChecked(toggleToolbox), "toggle toolbox button is checked");

  yield addTab("about:blank");
  info("Opened a new tab");

  ok(!isChecked(toggleToolbox), "toggle toolbox button is not checked");

  gBrowser.removeCurrentTab();

  let hidden = oneTimeObserve(toolbar.NOTIFICATIONS.HIDE);
  document.getElementById("menu_devToolbar").doCommand();
  yield hidden;
  ok(!toolbar.visible, "DeveloperToolbar is not visible in hidden");

  shown = oneTimeObserve(toolbar.NOTIFICATIONS.SHOW);
  document.getElementById("menu_devToolbar").doCommand();
  yield shown;
  ok(toolbar.visible, "DeveloperToolbar is visible in after open");

  ok(isChecked(toggleToolbox), "toggle toolbox button is checked");

  hidden = oneTimeObserve(toolbar.NOTIFICATIONS.HIDE);
  document.getElementById("developer-toolbar-closebutton").doCommand();
  yield hidden;

  ok(!toolbar.visible, "DeveloperToolbar is not visible after re-close");
});

function isChecked(b) {
  return b.getAttribute("checked") == "true";
}
