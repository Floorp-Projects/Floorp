/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that emptying out an existing value removes the property and
// doesn't cause any other issues.  See also Bug 1150780.

let TEST_URI = [
  '<style type="text/css">',
  '#testid {',
  '  color: red;',
  '  background-color: blue;',
  '  font-size: 12px;',
  '}',
  '.testclass, .unmatched {',
  '  background-color: green;',
  '}',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>',
  '<div id="testid2">Styled Node</div>'
].join("\n");

add_task(function*() {
  let tab = yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let doc = ruleEditor.doc;

  let propEditor = ruleEditor.rule.textProps[1].editor;
  let editor = yield focusEditableField(view, propEditor.valueSpan);

  info("Deleting all the text out of a value field");
  yield sendCharsAndWaitForFocus(view, ruleEditor.element, ["VK_DELETE", "VK_RETURN"]);

  info("Pressing enter a couple times to cycle through editors");
  yield sendCharsAndWaitForFocus(view, ruleEditor.element, ["VK_RETURN"]);
  yield sendCharsAndWaitForFocus(view, ruleEditor.element, ["VK_RETURN"]);

  isnot (ruleEditor.rule.textProps[1].editor.nameSpan.style.display, "none", "The name span is visible");
  is (ruleEditor.rule.textProps.length, 2, "Correct number of props");
});

function* sendCharsAndWaitForFocus(view, element, chars) {
  let onFocus = once(element, "focus", true);
  for (let ch of chars) {
    let onRuleViewChanged = view.once("ruleview-changed");
    EventUtils.sendChar(ch, element.ownerDocument.defaultView);
    yield onRuleViewChanged;
  }
  yield onFocus;
}
