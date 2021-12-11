/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that focus stays inside color picker on TAB and Shift + TAB

const TEST_URI = `
  <style type="text/css">
    body {
      color: red;
      background-color: #ededed;
      background-image: url(chrome://branding/content/icon64.png);
      border: 2em solid rgba(120, 120, 120, .5);
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();

  info("Focus on the property value span");
  getRuleViewProperty(view, "body", "color").valueSpan.focus();

  const cPicker = view.tooltips.getTooltip("colorPicker");
  const onColorPickerReady = cPicker.once("ready");

  info(
    "Tab to focus on the color swatch and press enter to simulate a click event"
  );
  EventUtils.sendKey("Tab");
  EventUtils.sendKey("Return");

  await onColorPickerReady;
  const doc = cPicker.spectrum.element.ownerDocument;
  ok(
    doc.activeElement.classList.contains("spectrum-color"),
    "Focus is initially on the spectrum dragger when color picker is shown."
  );

  info("Test that tabbing should move focus to the next focusable elements.");
  testFocusOnTab(doc, "devtools-button");
  testFocusOnTab(doc, "spectrum-hue-input");
  testFocusOnTab(doc, "spectrum-alpha-input");
  testFocusOnTab(doc, "learn-more");

  info(
    "Test that tabbing on the last element wraps focus to the first element."
  );
  testFocusOnTab(doc, "spectrum-color");

  info(
    "Test that shift tabbing on the first element wraps focus to the last element."
  );
  testFocusOnTab(doc, "learn-more", true);

  info(
    "Test that shift tabbing should move focus to the previous focusable elements."
  );
  testFocusOnTab(doc, "spectrum-alpha-input", true);
  testFocusOnTab(doc, "spectrum-hue-input", true);
  testFocusOnTab(doc, "devtools-button", true);
  testFocusOnTab(doc, "spectrum-color", true);

  await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
});

function testFocusOnTab(doc, expectedClass, shiftKey = false) {
  EventUtils.synthesizeKey("VK_TAB", { shiftKey });
  ok(
    doc.activeElement.classList.contains(expectedClass),
    "Focus is on the correct element."
  );
}
