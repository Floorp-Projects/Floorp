/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test stacktrace scratchpad linking</p>";

add_task(async function() {
  await pushPref("devtools.scratchpad.enabled", true);
  await openNewTabAndToolbox(TEST_URI);

  info("Opening toolbox with Scratchpad panel");

  const target = TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = await gDevTools.showToolbox(target, "scratchpad", "window");

  const scratchpadPanel = toolbox.getPanel("scratchpad");
  const { scratchpad } = scratchpadPanel;
  is(toolbox.getCurrentPanel(), scratchpadPanel,
    "Scratchpad is currently selected panel");

  info("Switching to webconsole panel");

  const webconsolePanel = await toolbox.selectTool("webconsole");
  const { hud } = webconsolePanel;
  is(toolbox.getCurrentPanel(), webconsolePanel,
    "Webconsole is currently selected panel");

  info("console.trace()ing from Scratchpad");

  scratchpad.setText(`
    function foo() {
      bar();
    }

    function bar() {
      console.trace();
    }

    foo();
  `);
  scratchpad.run();
  const message = await waitFor(() => findMessage(hud, "console.trace()"));

  info("Clicking link to switch to and focus Scratchpad");

  ok(message, "Found console.trace message from Scratchpad");
  const anchor = message.querySelector(".stack-trace .frame-link .frame-link-filename");

  const onScratchpadSelected = toolbox.once("scratchpad-selected");

  EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);
  await onScratchpadSelected;

  is(toolbox.getCurrentPanel(), scratchpadPanel,
    "Clicking link in stacktrace switches to Scratchpad panel");

  is(Services.ww.activeWindow, toolbox.win.parent,
     "Scratchpad's toolbox is focused");
});
