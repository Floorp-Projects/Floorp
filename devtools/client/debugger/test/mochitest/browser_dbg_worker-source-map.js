/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "doc_worker-source-map.html";
const WORKER_URL = "code_worker-source-map.js";
const COFFEE_URL = EXAMPLE_URL + "code_worker-source-map.coffee";

function selectWorker(aPanel, aURL) {
  let panelWin = aPanel.panelWin;
  let promise = waitForDebuggerEvents(aPanel, panelWin.EVENTS.WORKER_SELECTED);
  let Workers = panelWin.DebuggerView.Workers;
  let item = Workers.getItemForAttachment((workerForm) => {
    return workerForm.url === aURL;
  });
  Workers.selectedItem = item;
  return promise;
}

function test() {
  return Task.spawn(function* () {
    yield pushPrefs(["devtools.debugger.workers", true]);

    let options = {
      source: TAB_URL,
      line: 1
    };
    let [tab,, panel] = yield initDebugger(TAB_URL, options);
    let toolbox = yield selectWorker(panel, WORKER_URL);
    let workerPanel = toolbox.getCurrentPanel();
    yield waitForSourceShown(workerPanel, ".coffee");
    let panelWin = workerPanel.panelWin;
    let Sources = panelWin.DebuggerView.Sources;
    let editor = panelWin.DebuggerView.editor;
    let threadClient = panelWin.gThreadClient;

    isnot(Sources.selectedItem.attachment.source.url.indexOf(".coffee"), -1,
          "The debugger should show the source mapped coffee source file.");
    is(Sources.selectedValue.indexOf(".js"), -1,
       "The debugger should not show the generated js source file.");
    is(editor.getText().indexOf("isnt"), 211,
       "The debugger's editor should have the coffee source source displayed.");
    is(editor.getText().indexOf("function"), -1,
       "The debugger's editor should not have the JS source displayed.");

    yield threadClient.interrupt();
    let sourceForm = getSourceForm(Sources, COFFEE_URL);
    let source = threadClient.source(sourceForm);
    let response = yield source.setBreakpoint({ line: 5 });

    ok(!response.error,
      "Should be able to set a breakpoint in a coffee source file.");
    ok(!response.actualLocation,
      "Should be able to set a breakpoint on line 5.");

    let promise = new Promise((resolve) => {
      threadClient.addOneTimeListener("paused", (event, packet) => {
        is(packet.type, "paused",
          "We should now be paused again.");
        is(packet.why.type, "breakpoint",
          "and the reason we should be paused is because we hit a breakpoint.");

        // Check that we stopped at the right place, by making sure that the
        // environment is in the state that we expect.
        is(packet.frame.environment.bindings.variables.start.value, 0,
           "'start' is 0.");
        is(packet.frame.environment.bindings.variables.stop.value.type, "undefined",
           "'stop' hasn't been assigned to yet.");
        is(packet.frame.environment.bindings.variables.pivot.value.type, "undefined",
           "'pivot' hasn't been assigned to yet.");

        waitForCaretUpdated(workerPanel, 5).then(resolve);
      });
    });

    // This will cause the breakpoint to be hit, and put us back in the
    // paused state.
    yield threadClient.resume();
    callInTab(tab, "binary_search", [0, 2, 3, 5, 7, 10], 5);
    yield promise;

    yield threadClient.resume();
    yield toolbox.destroy();
    yield closeDebuggerAndFinish(panel);

    yield popPrefs();
  });
}
