/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing selector inplace-editor behaviors in the rule-view with pseudo
// classes.

const TEST_URI = `
  <style type="text/css">
    .testclass {
      text-align: center;
    }
    #testid3::first-letter {
      text-decoration: "italic"
    }
  </style>
  <div id="testid">Styled Node</div>
  <span class="testclass">This is a span</span>
  <div class="testclass2">A</div>
  <div id="testid3">B</div>
`;

const PSEUDO_PREF = "devtools.inspector.show_pseudo_elements";

add_task(function*() {
  // Expand the pseudo-elements section by default.
  Services.prefs.setBoolPref(PSEUDO_PREF, true);

  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode(".testclass", inspector);
  yield testEditSelector(view, "div:nth-child(1)");

  info("Selecting the modified element");
  yield selectNode("#testid", inspector);
  yield checkModifiedElement(view, "div:nth-child(1)");

  info("Selecting the test element");
  yield selectNode("#testid3", inspector);
  yield testEditSelector(view, ".testclass2::first-letter");

  info("Selecting the modified element");
  yield selectNode(".testclass2", inspector);
  yield checkModifiedElement(view, ".testclass2::first-letter");

  // Reset the pseudo-elements section pref to its default value.
  Services.prefs.clearUserPref(PSEUDO_PREF);
});

function* testEditSelector(view, name) {
  info("Test editing existing selector fields");

  let idRuleEditor = getRuleViewRuleEditor(view, 1) ||
    getRuleViewRuleEditor(view, 1, 0);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, idRuleEditor.selectorText);

  is(inplaceEditor(idRuleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name: " + name);
  editor.input.value = name;

  info("Waiting for rule view to update");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  is(view._elementStyle.rules.length, 2, "Should have 2 rule.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");

  let newRuleEditor = getRuleViewRuleEditor(view, 1) ||
    getRuleViewRuleEditor(view, 1, 0);
  ok(newRuleEditor.element.getAttribute("unmatched"),
    "Rule with " + name + " does not match the current element.");
}

function* checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}
