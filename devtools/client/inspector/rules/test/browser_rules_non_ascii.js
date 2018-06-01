/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that rule view can open when there are non-ASCII characters in
// the style sheet.  Regression test for bug 1390455.

// Use a few 4-byte UTF-8 sequences to make it so the rule column
// would be wrong when we had the bug.
const SHEET_TEXT = "/*🆒🆒🆒🆒🆒🆒🆒🆒🆒🆒🆒🆒🆒🆒🆒*/#q{color:orange}";
const HTML = `<style type="text/css">\n${SHEET_TEXT}
              </style><div id="q">Styled Node</div>`;
const TEST_URI = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

add_task(async function() {
  await addTab(TEST_URI);

  const {inspector, view} = await openRuleView();
  await selectNode("#q", inspector);

  const elementStyle = view._elementStyle;

  const expected = [
    {name: "color", overridden: false},
  ];

  const rule = elementStyle.rules[1];

  for (let i = 0; i < expected.length; ++i) {
    const prop = rule.textProps[i];
    is(prop.name, expected[i].name, `Got expected property name ${prop.name}`);
    is(prop.overridden, expected[i].overridden,
       `Got expected overridden value ${prop.overridden}`);
  }
});
