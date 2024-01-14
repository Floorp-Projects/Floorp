/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests color pickers displays expected contrast ratio information.

const TEST_URI = `
  <style type="text/css">
    :root {
      --title-color: #000;
    }

    body {
      color: #eee;
      background-color: #eee;
    }

    h1 {
      color: var(--title-color);
    }

    div {
      color: var(--title-color);
      /* Try to to have consistent results over different platforms:
         - using hardstop-ish gradient so the min and max contrast are computed against white and black background
         - having min-content width will make sure we get the gradient only cover the text, and not the whole screen width
      */
      background-image: linear-gradient(to right, black, white);
      width: min-content;
      font-size: 100px;
    }

    section {
      color: color-mix(in srgb, blue, var(--title-color) 50%);
    }
  </style>
  <h1>Testing the color picker contrast ratio data</h1>
  <div>————</div>
  <section>mixed colors</section>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await checkColorPickerConstrastData({
    view,
    label: "Displays contrast information on color property",
    ruleViewPropertyEl: getRuleViewProperty(view, "body", "color"),
    expectVisibleContrast: true,
    expectedContrastValueResult: "FAIL",
    expectedContrastValueTitle:
      "Does not meet WCAG standards for accessible text. Calculated against background: rgba(238,238,238,1)",
    expectedContrastValueScore: "1.00",
  });

  await checkColorPickerConstrastData({
    view,
    label: "Does not display contrast information on background-color property",
    ruleViewPropertyEl: getRuleViewProperty(view, "body", "background-color"),
    expectVisibleContrast: false,
  });

  await selectNode("h1", inspector);
  await checkColorPickerConstrastData({
    view,
    label: "Displays contrast information on color from CSS variable",
    ruleViewPropertyEl: getRuleViewProperty(view, "h1", "color"),
    expectVisibleContrast: true,
    expectedContrastValueResult: "AAA",
    expectedContrastValueTitle:
      "Meets WCAG AAA standards for accessible text. Calculated against background: rgba(238,238,238,1)",
    expectedContrastValueScore: "18.10",
  });

  await selectNode("div", inspector);
  await checkColorPickerConstrastData({
    view,
    label:
      "Displays range contrast information on color against gradient background",
    ruleViewPropertyEl: getRuleViewProperty(view, "div", "color"),
    expectVisibleContrast: true,
    expectContrastRange: true,
    expectedMinContrastValueResult: "FAIL",
    expectedMinContrastValueTitle:
      "Does not meet WCAG standards for accessible text. Calculated against background: rgba(0,0,0,1)",
    expectedMinContrastValueScore: "1.00",
    expectedMaxContrastValueResult: "AAA",
    expectedMaxContrastValueTitle:
      "Meets WCAG AAA standards for accessible text. Calculated against background: rgba(255,255,255,1)",
    expectedMaxContrastValueScore: "19.77",
  });

  await selectNode("section", inspector);
  await checkColorPickerConstrastData({
    view,
    label:
      "Does not displays contrast information on color within color-mix function (#1)",
    ruleViewPropertyEl: getRuleViewProperty(view, "section", "color"),
    swatchIndex: 0,
    expectVisibleContrast: false,
  });
  await checkColorPickerConstrastData({
    view,
    label:
      "Does not displays contrast information on color within color-mix function (#2)",
    ruleViewPropertyEl: getRuleViewProperty(view, "section", "color"),
    swatchIndex: 1,
    expectVisibleContrast: false,
  });
});

async function checkColorPickerConstrastData({
  view,
  ruleViewPropertyEl,
  label,
  swatchIndex = 0,
  expectVisibleContrast,
  expectedContrastValueResult,
  expectedContrastValueTitle,
  expectedContrastValueScore,
  expectContrastRange = false,
  expectedMinContrastValueResult,
  expectedMinContrastValueTitle,
  expectedMinContrastValueScore,
  expectedMaxContrastValueResult,
  expectedMaxContrastValueTitle,
  expectedMaxContrastValueScore,
}) {
  info(`Checking color picker: "${label}"`);
  const cPicker = view.tooltips.getTooltip("colorPicker");
  ok(cPicker, "The rule-view has an expected colorPicker widget");
  const cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  const colorSwatch = ruleViewPropertyEl.valueSpan.querySelectorAll(
    ".ruleview-colorswatch"
  )[swatchIndex];

  const onColorPickerReady = cPicker.once("ready");
  colorSwatch.click();
  await onColorPickerReady;
  ok(true, "The color picker was displayed");

  const contrastEl = cPickerPanel.querySelector(".spectrum-color-contrast");

  if (!expectVisibleContrast) {
    ok(
      !contrastEl.classList.contains("visible"),
      "Contrast information is not displayed, as expected"
    );
    await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
    return;
  }

  ok(
    contrastEl.classList.contains("visible"),
    "Contrast information is displayed"
  );
  is(
    contrastEl.classList.contains("range"),
    expectContrastRange,
    `Contrast information ${
      expectContrastRange ? "has" : "does not have"
    } a result range`
  );

  if (expectContrastRange) {
    const minContrastValueEl = contrastEl.querySelector(
      ".contrast-ratio-range .contrast-ratio-min .accessibility-contrast-value"
    );
    ok(
      minContrastValueEl.classList.contains(expectedMinContrastValueResult),
      `min contrast value has expected "${expectedMinContrastValueResult}" class`
    );
    // Scores vary from platform to platform, disable for now.
    // This should be re-enabled as part of  Bug 1780736
    // is(
    //   minContrastValueEl.title,
    //   expectedMinContrastValueTitle,
    //   "min contrast value has expected title"
    // );
    // is(
    //   minContrastValueEl.textContent,
    //   expectedMinContrastValueScore,
    //   "min contrast value shows expected score"
    // );

    const maxContrastValueEl = contrastEl.querySelector(
      ".contrast-ratio-range .contrast-ratio-max .accessibility-contrast-value"
    );
    ok(
      maxContrastValueEl.classList.contains(expectedMaxContrastValueResult),
      `max contrast value has expected "${expectedMaxContrastValueResult}" class`
    );
    // Scores vary from platform to platform, disable for now.
    // This should be re-enabled as part of  Bug 1780736
    // is(
    //   maxContrastValueEl.title,
    //   expectedMaxContrastValueTitle,
    //   "max contrast value has expected title"
    // );
    // is(
    //   maxContrastValueEl.textContent,
    //   expectedMaxContrastValueScore,
    //   "max contrast value shows expected score"
    // );
  } else {
    const contrastValueEl = contrastEl.querySelector(
      ".accessibility-contrast-value"
    );
    ok(
      contrastValueEl.classList.contains(expectedContrastValueResult),
      `contrast value has expected "${expectedContrastValueResult}" class`
    );
    is(
      contrastValueEl.title,
      expectedContrastValueTitle,
      "contrast value has expected title"
    );
    is(
      contrastValueEl.textContent,
      expectedContrastValueScore,
      "contrast value shows expected score"
    );
  }

  await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
}
