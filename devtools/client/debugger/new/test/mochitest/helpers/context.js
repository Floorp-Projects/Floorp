/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Helper method to create a "dbg" context for other tools to use
 */

function createDebuggerContext(toolbox) {
  const panel = toolbox.getPanel("jsdebugger");
  const win = panel.panelWin;
  const { store, client, selectors, actions } = panel.getVarsForTests();

  return {
    actions: actions,
    selectors: selectors,
    getState: store.getState,
    store: store,
    client: client,
    toolbox: toolbox,
    win: win,
    panel: panel
  };
}