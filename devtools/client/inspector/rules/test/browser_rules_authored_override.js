/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for as-authored styles.

async function createTestContent(style) {
  const html = `<style type="text/css">
      ${style}
      </style>
      <div id="testid" class="testclass">Styled Node</div>`;
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(html));

  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  return view;
}

add_task(async function() {
  const gradientText1 = "(orange, blue);";
  const gradientText2 = "(pink, teal);";

  const view = await createTestContent(
    "#testid {" +
      "  background-image: linear-gradient" +
      gradientText1 +
      "  background-image: -ms-linear-gradient" +
      gradientText2 +
      "  background-image: linear-gradient" +
      gradientText2 +
      "} "
  );

  const elementStyle = view._elementStyle;
  const rule = elementStyle.rules[1];

  // Initially the last property should be active.
  for (let i = 0; i < 3; ++i) {
    const prop = rule.textProps[i];
    is(prop.name, "background-image", "check the property name");
    is(prop.overridden, i !== 2, "check overridden for " + i);
  }

  await togglePropStatus(view, rule.textProps[2]);

  // Now the first property should be active.
  for (let i = 0; i < 3; ++i) {
    const prop = rule.textProps[i];
    is(
      prop.overridden || !prop.enabled,
      i !== 0,
      "post-change check overridden for " + i
    );
  }
});
