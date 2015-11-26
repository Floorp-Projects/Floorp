/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure javascript bookmarklet scripts appear and load correctly in the source list
 */

const TAB_URL = EXAMPLE_URL + "doc_script-bookmarklet.html";

const BOOKMARKLET_SCRIPT_CODE = "console.log('bookmarklet executed');";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;

    return Task.spawn(function*() {
      let waitForSource =  waitForDebuggerEvents(gPanel, gPanel.panelWin.EVENTS.NEW_SOURCE, 1);

      // NOTE: devtools debugger panel needs to be already open,
      // or the bookmarklet script will not be shown in the sources panel
      callInTab(gTab, "injectBookmarklet", BOOKMARKLET_SCRIPT_CODE);

      yield waitForSource;

      is(gSources.values.length, 2, "Should have 2 source");

      let item = gSources.getItemForAttachment(e => {
        return e.label.indexOf("javascript:") === 0;
      });
      ok(item, "Source label is incorrect.");

      let res = yield promiseInvoke(gDebugger.DebuggerController.client,
                                gDebugger.DebuggerController.client.request,
                                { to: item.value, type: "source"});

      ok(res && res.source == BOOKMARKLET_SCRIPT_CODE, "SourceActor reply received");
      is(res.source, BOOKMARKLET_SCRIPT_CODE, "source is correct");
      is(res.contentType, "text/javascript", "contentType is correct");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}
