/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure eval scripts appear in the source list
 */

const TAB_URL = EXAMPLE_URL + "doc_script-pagemod.html";

const pagemod = require("sdk/page-mod");
const CONTENT_SCRIPT_CODE = "console.log('page-mod executed');";


let testPageMod = pagemod.PageMod({
  include: TAB_URL,
  contentScript: CONTENT_SCRIPT_CODE
});

let destroyTestPageMod = testPageMod.destroy.bind(testPageMod);

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
      let waitForSource =  waitForDebuggerEvents(gPanel, gPanel.panelWin.EVENTS.SOURCES_ADDED, 1);

      // NOTE: devtools debugger panel needs to be already open,
      // or the pagemod script will not be shown in the sources panel
      callInTab(gTab, "reloadTab");

      yield waitForSource;
      is(gSources.values.length, 3, "Should have 3 source");

      let item = gSources.getItemForAttachment(e => {
        return e.label.indexOf("javascript:") === 0;
      });
      ok(item, "Source label is incorrect.");

      yield selectSourceAndGetBlackBoxButton(gPanel, "javascript:" + CONTENT_SCRIPT_CODE);

      let res = yield promiseInvoke(gDebugger.DebuggerController.client,
                                gDebugger.DebuggerController.client.request,
                                { to: item.value, type: "source"});

      ok(res && res.source == CONTENT_SCRIPT_CODE, "SourceActor reply received");
      is(res.source, CONTENT_SCRIPT_CODE, "source is correct");
      is(res.contentType, "text/javascript", "contentType is correct");

      yield closeDebuggerAndFinish(gPanel);
    }).then(destroyTestPageMod, destroyTestPageMod);
  });
}
