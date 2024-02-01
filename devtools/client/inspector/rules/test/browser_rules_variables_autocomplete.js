/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for autocomplete of CSS variables in the Rules view.

const IFRAME_URL = `https://example.org/document-builder.sjs?html=${encodeURIComponent(`
  <style>
    @property --iframe {
      syntax: "*";
      inherits: true;
    }
    body {
      --iframe-not-registered: turquoise;
    }

    h1 {
      color: tomato;
    }
  </style>
  <h1>iframe</h1>
`)}`;

const TEST_URI = `https://example.org/document-builder.sjs?html=
 <script>
    CSS.registerProperty({
      name: "--js",
      syntax: "*",
      inherits: false,
    });
  </script>
  <style>
    @property --css {
      syntax: "*";
      inherits: false;
    }

    h1 {
      --css: red;
      --not-registered: blue;
      color: gold;
    }
  </style>
  <h1>Hello world</h1>
  <iframe src="${encodeURIComponent(IFRAME_URL)}"></iframe>`;

add_task(async function () {
  await pushPref("layout.css.properties-and-values.enabled", true);

  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  info("Wait for @property panel to be displayed");
  await waitFor(() =>
    view.styleDocument.querySelector("#registered-properties-container")
  );

  const topLevelVariables = ["--css", "--js", "--not-registered"];
  await checkNewPropertyCssVariableAutocomplete(view, topLevelVariables);

  await checkCssVariableAutocomplete(
    view,
    getTextProperty(view, 1, { color: "gold" }).editor.valueSpan,
    topLevelVariables
  );

  info(
    "Check that the list is correct when selecting a node from another document"
  );
  await selectNodeInFrames(["iframe", "h1"], inspector);

  const iframeVariables = ["--iframe", "--iframe-not-registered"];
  await checkNewPropertyCssVariableAutocomplete(view, iframeVariables);

  await checkCssVariableAutocomplete(
    view,
    getTextProperty(view, 1, { color: "tomato" }).editor.valueSpan,
    iframeVariables
  );
});

async function checkNewPropertyCssVariableAutocomplete(
  view,
  expectedPopupItems
) {
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  const editor = await focusNewRuleViewProperty(ruleEditor);
  const onPopupOpen = editor.popup.once("popup-opened");
  EventUtils.sendString("--");
  await onPopupOpen;

  Assert.deepEqual(
    editor.popup.getItems().map(item => item.label),
    expectedPopupItems,
    "Got expected items in autopopup"
  );

  info("Close the popup");
  const onPopupClosed = once(editor.popup, "popup-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onPopupClosed;
}

async function checkCssVariableAutocomplete(
  view,
  inplaceEditorEl,
  expectedPopupItems
) {
  let onRuleViewChanged = view.once("ruleview-changed");
  const editor = await focusEditableField(view, inplaceEditorEl);
  const onPopupOpen = editor.popup.once("popup-opened");
  EventUtils.sendString("var(--");
  view.debounce.flush();
  await onPopupOpen;
  await onRuleViewChanged;
  Assert.deepEqual(
    editor.popup.getItems().map(item => item.label),
    expectedPopupItems,
    "Got expected items in autopopup"
  );

  info("Close the popup");
  const onPopupClosed = once(editor.popup, "popup-closed");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  view.debounce.flush();
  await onRuleViewChanged;
  await onPopupClosed;
}
