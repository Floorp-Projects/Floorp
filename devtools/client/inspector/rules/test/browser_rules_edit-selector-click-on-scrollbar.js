/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing ruleview inplace-editor is not blurred when clicking on the ruleview
// container scrollbar.

const TEST_URI = `
  <style type="text/css">
    div.testclass {
      color: black;
    }
    .a {
      color: #aaa;
    }
    .b {
      color: #bbb;
    }
    .c {
      color: #ccc;
    }
    .d {
      color: #ddd;
    }
    .e {
      color: #eee;
    }
    .f {
      color: #fff;
    }
  </style>
  <div class="testclass a b c d e f">Styled Node</div>
`;

add_task(function* () {
  info("Toolbox height should be small enough to force scrollbars to appear");
  yield new Promise(done => {
    let options = {"set": [
      ["devtools.toolbox.footer.height", 200],
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode(".testclass", inspector);

  info("Check we have an overflow on the ruleview container.");
  let container = view.element;
  let hasScrollbar = container.offsetHeight < container.scrollHeight;
  ok(hasScrollbar, "The rule view container should have a vertical scrollbar.");

  info("Focusing an existing selector name in the rule-view.");
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusEditableField(view, ruleEditor.selectorText);
  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor is focused.");

  info("Click on the scrollbar element.");
  yield clickOnRuleviewScrollbar(view);

  is(editor.input, view.styleDocument.activeElement,
    "The editor input should still be focused.");

  info("Check a new value can still be committed in the editable field");
  let newValue = ".testclass.a.b.c.d.e.f";
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Enter new value and commit.");
  editor.input.value = newValue;
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;
  ok(getRuleViewRule(view, newValue), "Rule with '" + newValue + " 'exists.");
});

function* clickOnRuleviewScrollbar(view) {
  let container = view.element.parentNode;
  let onScroll = once(container, "scroll");
  let rect = container.getBoundingClientRect();
  // click 5 pixels before the bottom-right corner should hit the scrollbar
  EventUtils.synthesizeMouse(container, rect.width - 5, rect.height - 5,
    {}, view.styleWindow);
  yield onScroll;

  ok(true, "The rule view container scrolled after clicking on the scrollbar.");
}
