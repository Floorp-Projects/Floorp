/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changes in the style inspector are synchronized into the
// style editor.

Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/inspector/shared/test/head.js", this);

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
  body {
    border-width: 15px;
    /*! color: red; */
  }

  #testid {
    /*! font-size: 4em; */
  }
  `;

function* closeAndReopenToolbox() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);
  let { ui: newui } = yield openStyleEditor();
  return newui;
}

add_task(function*() {
  yield addTab(TESTCASE_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#testid", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 1);

  // Disable the "font-size" property.
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let onModification = view.once("ruleview-changed");
  propEditor.enable.click();
  yield onModification;

  // Disable the "color" property.  Note that this property is in a
  // rule that also contains a non-inherited property -- so this test
  // is also testing that property editing works properly in this
  // situation.
  ruleEditor = getRuleViewRuleEditor(view, 3);
  propEditor = ruleEditor.rule.textProps[1].editor;
  onModification = view.once("ruleview-changed");
  propEditor.enable.click();
  yield onModification;

  let { ui } = yield openStyleEditor();

  let editor = yield ui.editors[0].getSourceEditor();
  let text = editor.sourceEditor.getText();
  is(text, expectedText, "style inspector changes are synced");

  // Close and reopen the toolbox, to see that the edited text remains
  // available.
  ui = yield closeAndReopenToolbox();
  editor = yield ui.editors[0].getSourceEditor();
  text = editor.sourceEditor.getText();
  is(text, expectedText, "changes remain after close and reopen");

  // For the time being, the actor does not update the style's owning
  // node's textContent.  See bug 1205380.
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    let style = content.document.querySelector("style");
    return style.textContent;
  }).then((textContent) => {
    isnot(textContent, expectedText, "changes not written back to style node");
  });
});
