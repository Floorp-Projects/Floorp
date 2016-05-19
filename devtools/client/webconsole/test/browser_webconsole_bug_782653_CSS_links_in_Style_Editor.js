/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/test" +
                 "/test-bug-782653-css-errors.html";

var nodes, hud, StyleEditorUI;

add_task(function* () {
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
      text: "\u2018font-weight\u2019",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    },
    {
      text: "\u2018color\u2019",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    }],
  }).then(([error1Rule, error2Rule]) => {
    let error1Msg = [...error1Rule.matched][0];
    let error2Msg = [...error2Rule.matched][0];
    nodes = [error1Msg.querySelector(".message-location .frame-link"),
             error2Msg.querySelector(".message-location .frame-link")];
    ok(nodes[0], ".frame-link node for the first error");
    ok(nodes[1], ".frame-link node for the second error");

    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);
    toolbox.once("styleeditor-selected", (event, panel) => {
      StyleEditorUI = panel.UI;
      deferred.resolve(panel);
    });

    EventUtils.sendMouseEvent({ type: "click" }, nodes[0].querySelector(".frame-link-filename"));
  });

  return deferred.promise;
}

function onStyleEditorReady(panel) {
  let deferred = promise.defer();

  let win = panel.panelWindow;
  ok(win, "Style Editor Window is defined");
  ok(StyleEditorUI, "Style Editor UI is defined");

  function fireEvent(toolbox, href, line) {
    toolbox.once("styleeditor-selected", function (evt) {
      info(evt + " event fired");

      checkStyleEditorForSheetAndLine(href, line - 1).then(deferred.resolve);
    });

    EventUtils.sendMouseEvent({ type: "click" }, nodes[1].querySelector(".frame-link-filename"));
  }

  waitForFocus(function () {
    info("style editor window focused");

    let href = nodes[0].getAttribute("data-url");
    let line = nodes[0].getAttribute("data-line");
    ok(line, "found source line");

    checkStyleEditorForSheetAndLine(href, line - 1).then(function () {
      info("first check done");

      let target = TargetFactory.forTab(gBrowser.selectedTab);
      let toolbox = gDevTools.getToolbox(target);

      href = nodes[1].getAttribute("data-url");
      line = nodes[1].getAttribute("data-line");
      ok(line, "found source line");

      toolbox.selectTool("webconsole").then(function () {
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
