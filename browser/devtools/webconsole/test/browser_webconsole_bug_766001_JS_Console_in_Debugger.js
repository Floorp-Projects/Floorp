/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test" +
                 "/test-bug-766001-js-console-links.html";

function test() {
  let hud;

  requestLongerTimeout(2);
  Task.spawn(runner).then(finishTest);

  function* runner() {
    expectUncaughtException();
    let {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    let [exceptionRule, consoleRule] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "document.bar",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        text: "Blah Blah",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    });

    let exceptionMsg = [...exceptionRule.matched][0];
    let consoleMsg = [...consoleRule.matched][0];
    let nodes = [exceptionMsg.querySelector(".location"),
                 consoleMsg.querySelector(".location")];
    ok(nodes[0], ".location node for the exception message");
    ok(nodes[1], ".location node for the console message");

    for (let i = 0; i < nodes.length; i++) {
      yield checkClickOnNode(i, nodes[i]);
      yield gDevTools.showToolbox(hud.target, "webconsole");
    }

    // check again the first node.
    yield checkClickOnNode(0, nodes[0]);
  }

  function* checkClickOnNode(index, node) {
    info("checking click on node index " + index);

    let url = node.getAttribute("title");
    ok(url, "source url found for index " + index);

    let line = node.sourceLine;
    ok(line, "found source line for index " + index);

    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" }, node);
    });

    yield hud.ui.once("source-in-debugger-opened", checkLine.bind(null, url, line));
  }

  function* checkLine(url, line) {
    let toolbox = yield gDevTools.getToolbox(hud.target);
    let {panelWin: { DebuggerView: view }} = toolbox.getPanel("jsdebugger");
    is(view.Sources.selectedValue, url, "expected source url");
    is(view.editor.getCursor().line, line - 1, "expected source line");
  }
}
