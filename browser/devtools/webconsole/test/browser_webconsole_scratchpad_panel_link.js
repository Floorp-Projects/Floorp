/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf8,<p>test Scratchpad panel linking</p>";

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Tools } = require("main");
let { isTargetSupported } = Tools.scratchpad;

Tools.scratchpad.isTargetSupported = () => true;


function test()
{
  waitForExplicitFinish();

  addTab(TEST_URI);
  gBrowser.selectedBrowser.addEventListener("load", function onTabLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onTabLoad, true);
    info("Opening toolbox with Scratchpad panel");

    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "scratchpad", "window").then(runTests);
  }, true);
}

function runTests(aToolbox)
{
  Task.spawn(function*() {
    let scratchpadPanel = aToolbox.getPanel("scratchpad");
    let { scratchpad } = scratchpadPanel;
    is(aToolbox.getCurrentPanel(), scratchpadPanel,
      "Scratchpad is currently selected panel");

    info("Switching to webconsole panel");

    let webconsolePanel = yield aToolbox.selectTool("webconsole");
    let { hud } = webconsolePanel;
    is(aToolbox.getCurrentPanel(), webconsolePanel,
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
    let anchor = matched.querySelector("a.message-location");

    aToolbox.on("scratchpad-selected", function selected() {
      aToolbox.off("scratchpad-selected", selected);

      is(aToolbox.getCurrentPanel(), scratchpadPanel,
        "Clicking link switches to Scratchpad panel");
      
      is(Services.ww.activeWindow, aToolbox.frame.ownerGlobal,
         "Scratchpad's toolbox is focused");

      Tools.scratchpad.isTargetSupported = isTargetSupported;
      finish();
    });

    EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);
  });
}
