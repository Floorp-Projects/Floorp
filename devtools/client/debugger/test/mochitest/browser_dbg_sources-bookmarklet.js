/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure javascript bookmarklet scripts appear and load correctly in the source list
 */

const TAB_URL = EXAMPLE_URL + "doc_script-bookmarklet.html";

const BOOKMARKLET_SCRIPT_CODE = "console.log('bookmarklet executed');";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    const getState = gDebugger.DebuggerController.getState;
    const constants = gDebugger.require('./content/constants');
    const queries = gDebugger.require('./content/queries');
    const actions = bindActionCreators(gPanel);

    return Task.spawn(function*() {
      yield waitForSourceShown(gPanel, '.html');

      const added =  waitForNextDispatch(gDebugger.DebuggerController, constants.ADD_SOURCE);
      // NOTE: devtools debugger panel needs to be already open,
      // or the bookmarklet script will not be shown in the sources panel
      callInTab(gTab, "injectBookmarklet", BOOKMARKLET_SCRIPT_CODE);
      yield added;

      is(queries.getSourceCount(getState()), 2, "Should have 2 sources");

      const sources = queries.getSources(getState());
      const sourceActor = Object.keys(sources).filter(k => {
        return sources[k].url.indexOf("javascript:") === 0;
      })[0];
      const source = sources[sourceActor];
      ok(source, "Source exists.");

      let res = yield actions.loadSourceText(source);
      is(res.text, BOOKMARKLET_SCRIPT_CODE, "source is correct");
      is(res.contentType, "text/javascript", "contentType is correct");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}
