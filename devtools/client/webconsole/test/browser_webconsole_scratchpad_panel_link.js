/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test Scratchpad panel " +
                 "linking</p>";

var { Tools } = require("devtools/client/definitions");
var { isTargetSupported } = Tools.scratchpad;

function pushPrefEnv() {
  let deferred = promise.defer();
  let options = {"set":
      [["devtools.scratchpad.enabled", true]
  ]};
  SpecialPowers.pushPrefEnv(options, deferred.resolve);
  return deferred.promise;
}

add_task(function*() {
  waitForExplicitFinish();

  yield pushPrefEnv();

  yield loadTab(TEST_URI);

  info("Opening toolbox with Scratchpad panel");

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "scratchpad", "window");

  let scratchpadPanel = toolbox.getPanel("scratchpad");
  let { scratchpad } = scratchpadPanel;
  is(toolbox.getCurrentPanel(), scratchpadPanel,
    "Scratchpad is currently selected panel");

  info("Switching to webconsole panel");

  let webconsolePanel = yield toolbox.selectTool("webconsole");
  let { hud } = webconsolePanel;
  is(toolbox.getCurrentPanel(), webconsolePanel,
    "Webconsole is currently selected panel");

  info("console.log()ing from Scratchpad");

  scratchpad.setText("console.log('foobar-from-scratchpad')");
  scratchpad.run();
  let messages = yield waitForMessages({
    webconsole: hud,
    messages: [{ text: "foobar-from-scratchpad" }]
  });

  info("Clicking link to switch to and focus Scratchpad");

  let [matched] = [...messages[0].matched];
  ok(matched, "Found logged message from Scratchpad");
  let anchor = matched.querySelector(".message-location .frame-link-filename");

  toolbox.on("scratchpad-selected", function selected() {
    toolbox.off("scratchpad-selected", selected);

    is(toolbox.getCurrentPanel(), scratchpadPanel,
      "Clicking link switches to Scratchpad panel");

    is(Services.ww.activeWindow, toolbox.frame.ownerGlobal,
       "Scratchpad's toolbox is focused");

    Tools.scratchpad.isTargetSupported = isTargetSupported;
    finish();
  });

  EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);
});
