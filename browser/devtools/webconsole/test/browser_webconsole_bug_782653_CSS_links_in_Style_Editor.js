/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test" +
                 "/test-bug-782653-css-errors.html";

let nodes, hud, StyleEditorUI;

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  hud = yield openConsole();

  let styleEditor = yield testViewSource();
  yield onStyleEditorReady(styleEditor);

  nodes = hud = StyleEditorUI = null;
});

function testViewSource() {
  let deferred = promise.defer();

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "'font-weight'",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    },
    {
      text: "'color'",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    }],
  }).then(([error1Rule, error2Rule]) => {
    let error1Msg = [...error1Rule.matched][0];
    let error2Msg = [...error2Rule.matched][0];
    nodes = [error1Msg.querySelector(".message-location"),
             error2Msg.querySelector(".message-location")];
    ok(nodes[0], ".message-location node for the first error");
    ok(nodes[1], ".message-location node for the second error");

    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);
    toolbox.once("styleeditor-selected", (event, panel) => {
      StyleEditorUI = panel.UI;
      deferred.resolve(panel);
    });

    EventUtils.sendMouseEvent({ type: "click" }, nodes[0]);
  });

  return deferred.promise;
}

function onStyleEditorReady(panel) {
  let deferred = promise.defer();

  let win = panel.panelWindow;
  ok(win, "Style Editor Window is defined");
  ok(StyleEditorUI, "Style Editor UI is defined");

  function fireEvent(toolbox, href, line) {
    toolbox.once("styleeditor-selected", function(evt) {
      info(evt + " event fired");

      checkStyleEditorForSheetAndLine(href, line - 1).then(deferred.resolve);
    });

    EventUtils.sendMouseEvent({ type: "click" }, nodes[1]);
  }

  waitForFocus(function() {
    info("style editor window focused");

    let href = nodes[0].getAttribute("title");
    let line = nodes[0].sourceLine;
    ok(line, "found source line");

    checkStyleEditorForSheetAndLine(href, line - 1).then(function() {
      info("first check done");

      let target = TargetFactory.forTab(gBrowser.selectedTab);
      let toolbox = gDevTools.getToolbox(target);

      href = nodes[1].getAttribute("title");
      line = nodes[1].sourceLine;
      ok(line, "found source line");

      toolbox.selectTool("webconsole").then(function() {
        info("webconsole selected");
        fireEvent(toolbox, href, line);
      });
    });
  }, win);

  return deferred.promise;
}

function checkStyleEditorForSheetAndLine(href, line) {
  let foundEditor = null;
  for (let editor of StyleEditorUI.editors) {
    if (editor.styleSheet.href == href) {
      foundEditor = editor;
      break;
    }
  }

  ok(foundEditor, "found style editor for " + href);
  return performLineCheck(foundEditor, line);
}

function performLineCheck(editor, line) {
  let deferred = promise.defer();

  function checkForCorrectState() {
    is(editor.sourceEditor.getCursor().line, line,
       "correct line is selected");
    is(StyleEditorUI.selectedStyleSheetIndex, editor.styleSheet.styleSheetIndex,
       "correct stylesheet is selected in the editor");

    executeSoon(deferred.resolve);
  }

  info("wait for source editor to load");

  // Get out of the styleeditor-selected event loop.
  executeSoon(() => {
    editor.getSourceEditor().then(() => {
      // Get out of the editor's source-editor-load event loop.
      executeSoon(checkForCorrectState);
    });
  });

  return deferred.promise;
}
