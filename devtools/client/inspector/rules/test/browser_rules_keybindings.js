/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test keyboard navigation in the rule view

add_task(async function () {
  await pushPref("devtools.inspector.showRulesViewEnterKeyNotice", true);
  const tab = await addTab(`data:text/html;charset=utf-8,
    <style>h1 {}</style>
    <h1>Some header text</h1>`);
  let { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  let kbdNoticeEl = view.styleDocument.getElementById(
    "ruleview-kbd-enter-notice"
  );
  ok(kbdNoticeEl.hasAttribute("hidden"), "Notice is not displayed by default");

  info("Getting the ruleclose brace element for the `h1` rule");
  const brace = view.styleDocument.querySelectorAll(".ruleview-ruleclose")[1];

  info("Focus the new property editable field to create a color property");
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  await focusNewRuleViewProperty(ruleEditor);
  EventUtils.sendString("color");

  info("Typing ENTER to focus the next field: property value");
  let onFocus = once(brace.parentNode, "focus", true);
  let onRuleViewChanged = view.once("ruleview-changed");

  EventUtils.sendKey("Return");

  await onFocus;
  await onRuleViewChanged;
  ok(true, "The value field was focused");

  info("Entering a property value");
  EventUtils.sendString("tomato");

  info("Typing Tab again should focus a new property name");
  onFocus = once(brace.parentNode, "focus", true);
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("Tab");
  await onFocus;
  await onRuleViewChanged;
  ok(true, "The new property name field was focused");

  info(
    "Filling new property name with background-color and hit Tab to focus value input"
  );
  EventUtils.sendString("background-color");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("Tab");
  await onRuleViewChanged;

  ok(true, "The value field was focused");

  info("Entering a background color value");
  EventUtils.sendString("gold");

  info("Typing Enter should close the input and focus the value span");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("Return");
  await onRuleViewChanged;

  info("Wait until the swatch for the color is created");
  const colorSwatchEl = await waitFor(() =>
    getRuleViewProperty(
      view,
      "h1",
      "background-color"
    )?.valueSpan?.querySelector(".ruleview-colorswatch")
  );

  ok(
    !kbdNoticeEl.hasAttribute("hidden"),
    "Notice is displayed after hitting Enter"
  );

  info("Click on dismiss button");
  kbdNoticeEl.querySelector("button").click();
  ok(
    kbdNoticeEl.hasAttribute("hidden"),
    "Notice was hidden after clicking on dismiss button"
  );

  is(
    view.styleDocument.activeElement.textContent,
    "gold",
    "Value span is focused after pressing Enter"
  );

  info("Type Tab should focus the color swatch");
  EventUtils.sendKey("Tab");
  is(
    view.styleDocument.activeElement,
    colorSwatchEl,
    "Focused was moved to color swatch"
  );

  info("Press Shift Tab");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(
    view.styleDocument.activeElement.textContent,
    "gold",
    "Focus is moved back to property value"
  );

  info("Press Shift Tab again");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(
    view.styleDocument.activeElement.textContent,
    "background-color",
    "Focus is moved back to property name"
  );

  info("Press Shift Tab once more");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  ok(
    view.styleDocument.activeElement.matches(
      "input[type=checkbox].ruleview-enableproperty"
    ),
    "Focus is moved to the prop toggle checkbox"
  );
  const toggleEl = view.styleDocument.activeElement;
  ok(toggleEl.checked, "Checkbox is checked by default");
  is(
    toggleEl.getAttribute("title"),
    "Enable background-color property",
    "checkbox has expected label"
  );

  info("Press Space to uncheck checkbox");
  let onRuleViewRefreshed = view.once("ruleview-changed");
  EventUtils.sendKey("Space");
  await onRuleViewRefreshed;
  ok(!toggleEl.checked, "Checkbox is now unchecked");

  info("Press Space to check checkbox back");
  onRuleViewRefreshed = view.once("ruleview-changed");
  EventUtils.sendKey("Space");
  await onRuleViewRefreshed;
  ok(toggleEl.checked, "Checkbox is checked again");

  info("Re-start the toolbox");
  await gDevTools.closeToolboxForTab(tab);
  ({ view } = await openRuleView());

  kbdNoticeEl = view.styleDocument.getElementById("ruleview-kbd-enter-notice");
  ok(
    kbdNoticeEl.hasAttribute("hidden"),
    "Notice isn't displayed on init when it was dismissed before"
  );
  is(
    Services.prefs.getBoolPref(
      "devtools.inspector.showRulesViewEnterKeyNotice"
    ),
    false,
    "The preference driving the UI is set to false, as expected"
  );
  Services.prefs.clearUserPref(
    "devtools.inspector.showRulesViewEnterKeyNotice"
  );
});

// The `element` have specific behavior, so we want to test that keyboard navigation
// also works fine on them.

add_task(async function testKeyboardNavigationInElementRule() {
  await addTab("data:text/html;charset=utf-8,<h1>Some header text</h1>");
  const { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  info("Getting the ruleclose brace element");
  const brace = view.styleDocument.querySelector(".ruleview-ruleclose");

  info("Focus the new property editable field to create a color property");
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  let editor = await focusNewRuleViewProperty(ruleEditor);
  editor.input.value = "color";

  info("Typing ENTER to focus the next field: property value");
  let onFocus = once(brace.parentNode, "focus", true);
  let onRuleViewChanged = view.once("ruleview-changed");
  let onStyleAttributeMutation = waitForStyleAttributeMutation(view, `color: `);

  EventUtils.sendKey("Return");

  await onFocus;
  await onRuleViewChanged;
  await onStyleAttributeMutation;
  ok(true, "The value field was focused");

  info("Entering a property value");
  onStyleAttributeMutation = waitForStyleAttributeMutation(
    view,
    `color: green;`
  );
  editor = getCurrentInplaceEditor(view);
  editor.input.value = "green";

  info("Typing Tab again should focus a new property name");
  onFocus = once(brace.parentNode, "focus", true);
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("Tab");
  await onFocus;
  await onRuleViewChanged;
  await onStyleAttributeMutation;
  ok(true, "The new property name field was focused");

  info(
    "Filling new property name with background-color and hit Tab to focus value input"
  );

  EventUtils.sendString("background-color");

  onRuleViewChanged = view.once("ruleview-changed");
  onStyleAttributeMutation = waitForStyleAttributeMutation(
    view,
    `background-color:`
  );
  EventUtils.sendKey("Tab");
  await onRuleViewChanged;
  await onStyleAttributeMutation;

  ok(true, "The value field was focused");

  info("Entering a background color value");
  onStyleAttributeMutation = waitForStyleAttributeMutation(
    view,
    `background-color: tomato;`
  );

  EventUtils.sendString("tomato", view.styleWindow);

  info("Typing Enter should close the input and focus the value span");
  const onValueDone = view.once("ruleview-changed");
  // The element rule is reset when a property is added, which impacts how we deal
  // with the focused element.
  const onRuleEditorFocusReset = view.once("rule-editor-focus-reset");
  EventUtils.sendKey("Return");

  await onValueDone;
  await onRuleEditorFocusReset;
  await onStyleAttributeMutation;

  is(
    view.styleDocument.activeElement,
    getRuleViewProperty(view, "element", "background-color").valueSpan,
    `background-color value span ("tomato") is focused after pressing Enter`
  );
  is(
    view.styleDocument.activeElement.textContent,
    "tomato",
    `focused element has expected text`
  );
});

function waitForStyleAttributeMutation(view, expectedAttributeValue) {
  return new Promise(r => {
    view.inspector.walker.on(
      "mutations",
      function onWalkerMutations(mutations) {
        // Wait until we receive a mutation which updates the style attribute
        // with the expected value.
        const receivedLastMutation = mutations.find(
          mut =>
            mut.attributeName === "style" &&
            mut.newValue.includes(expectedAttributeValue)
        );
        if (receivedLastMutation) {
          view.inspector.walker.off("mutations", onWalkerMutations);
          r();
        }
      }
    );
  });
}

function getCurrentInplaceEditor(view) {
  return inplaceEditor(view.styleDocument.activeElement);
}
