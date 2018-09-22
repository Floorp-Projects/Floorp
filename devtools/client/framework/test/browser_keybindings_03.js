/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the toolbox 'switch to previous host' feature works.
// Pressing ctrl/cmd+shift+d should switch to the last used host.

const URL = "data:text/html;charset=utf8,test page for toolbox switching";

var {Toolbox} = require("devtools/client/framework/toolbox");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

add_task(async function() {
  info("Create a test tab and open the toolbox");
  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "webconsole");

  const shortcut = L10N.getStr("toolbox.toggleHost.key");

  const {RIGHT, BOTTOM, WINDOW} = Toolbox.HostType;
  checkHostType(toolbox, BOTTOM, RIGHT);

  info("Switching from bottom to right");
  let onHostChanged = toolbox.once("host-changed");
  synthesizeKeyShortcut(shortcut, toolbox.win);
  await onHostChanged;
  checkHostType(toolbox, RIGHT, BOTTOM);

  info("Switching from right to bottom");
  onHostChanged = toolbox.once("host-changed");
  synthesizeKeyShortcut(shortcut, toolbox.win);
  await onHostChanged;
  checkHostType(toolbox, BOTTOM, RIGHT);

  info("Switching to window");
  await toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW, BOTTOM);

  info("Switching from window to bottom");
  onHostChanged = toolbox.once("host-changed");
  synthesizeKeyShortcut(shortcut, toolbox.win);
  await onHostChanged;
  checkHostType(toolbox, BOTTOM, WINDOW);

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});
