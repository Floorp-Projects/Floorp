/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

var {Toolbox} = require("devtools/client/framework/toolbox");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

add_task(async function() {
  const tab = await addTab("about:blank");
  const target = TargetFactory.forTab(tab);
  await target.makeRemote();

  const toolIDs = gDevTools.getToolDefinitionArray()
                         .filter(
                           def =>
                             def.isTargetSupported(target) &&
                             def.id !== "options"
                         )
                         .map(def => def.id);

  const toolbox = await gDevTools.showToolbox(target, toolIDs[0], Toolbox.HostType.BOTTOM);
  const nextShortcut = L10N.getStr("toolbox.nextTool.key");
  const prevShortcut = L10N.getStr("toolbox.previousTool.key");

  // Iterate over all tools, starting from options to netmonitor, in normal
  // order.
  for (let i = 1; i < toolIDs.length; i++) {
    await testShortcuts(toolbox, i, nextShortcut, toolIDs);
  }

  // Iterate again, in the same order, starting from netmonitor (so next one is
  // 0: options).
  for (let i = 0; i < toolIDs.length; i++) {
    await testShortcuts(toolbox, i, nextShortcut, toolIDs);
  }

  // Iterate over all tools in reverse order, starting from netmonitor to
  // options.
  for (let i = toolIDs.length - 2; i >= 0; i--) {
    await testShortcuts(toolbox, i, prevShortcut, toolIDs);
  }

  // Iterate again, in reverse order again, starting from options (so next one
  // is length-1: netmonitor).
  for (let i = toolIDs.length - 1; i >= 0; i--) {
    await testShortcuts(toolbox, i, prevShortcut, toolIDs);
  }

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

async function testShortcuts(toolbox, index, shortcut, toolIDs) {
  info("Testing shortcut to switch to tool " + index + ":" + toolIDs[index] +
       " using shortcut " + shortcut);

  const onToolSelected = toolbox.once("select");
  synthesizeKeyShortcut(shortcut);
  const id = await onToolSelected;

  info("toolbox-select event from " + id);

  is(toolIDs.indexOf(id), index,
     "Correct tool is selected on pressing the shortcut for " + id);
}
