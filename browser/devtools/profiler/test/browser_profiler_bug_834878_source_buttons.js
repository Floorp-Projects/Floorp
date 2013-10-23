/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const BASE = "http://example.com/browser/browser/devtools/profiler/test/";
const URL = BASE + "mock_profiler_bug_834878_page.html";
const SCRIPT = BASE + "mock_profiler_bug_834878_script.js";

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    let data = { uri: SCRIPT, line: 5, isChrome: false };

    panel.displaySource(data).then(function onOpen() {
      let target = TargetFactory.forTab(tab);
      let dbg = gDevTools.getToolbox(target).getPanel("jsdebugger");
      let view = dbg.panelWin.DebuggerView;

      is(view.Sources.selectedValue, data.uri, "URI is different");
      is(view.editor.getCursor().line, data.line - 1, "Line is different");

      // Test the case where script is already loaded.
      view.editor.setCursor({ line: 1, ch: 1 });
      gDevTools.showToolbox(target, "jsprofiler").then(function () {
        panel.displaySource(data).then(function onOpenAgain() {
          is(view.editor.getCursor().line, data.line - 1,
            "Line is different");
          tearDown(tab);
        });
      });
    });
  });
}
