/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for as-authored styles.

async function createTestContent(style) {
  let html = `<style type="text/css">
      ${style}
      </style>
      <div id="testid" class="testclass">Styled Node</div>`;
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(html));

  let {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  return view;
}

add_task(async function() {
  let view = await createTestContent("#testid {" +
                                     // Invalid property.
                                     "  something: random;" +
                                     // Invalid value.
                                     "  color: orang;" +
                                     // Override.
                                     "  background-color: blue;" +
                                     "  background-color: #f06;" +
                                     "} ");

  let elementStyle = view._elementStyle;

  let expected = [
    {name: "something", overridden: true},
    {name: "color", overridden: true},
    {name: "background-color", overridden: true},
    {name: "background-color", overridden: false}
  ];

  let rule = elementStyle.rules[1];

  for (let i = 0; i < expected.length; ++i) {
    let prop = rule.textProps[i];
    is(prop.name, expected[i].name, "test name for prop " + i);
    is(prop.overridden, expected[i].overridden,
       "test overridden for prop " + i);
  }
});
