/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Helper method to create a "dbg" context for other tools to use
 */

function createDebuggerContext(toolbox) {
  const panel = toolbox.getPanel("jsdebugger");
  const win = panel.panelWin;

  return {
    ...win.dbg,
    commands: toolbox.commands,
    toolbox: toolbox,
    win: win,
    panel: panel
  };
}
