/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for as-authored styles.

function* createTestContent(style) {
  let html = `<style type="text/css">
      ${style}
      </style>
      <div id="testid" class="testclass">Styled Node</div>`;
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(html));

  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  return view;
}

add_task(function* () {
  let gradientText1 = "(orange, blue);";
  let gradientText2 = "(pink, teal);";

  let view =
      yield createTestContent("#testid {" +
                              "  background-image: linear-gradient" +
                              gradientText1 +
                              "  background-image: -ms-linear-gradient" +
                              gradientText2 +
                              "  background-image: linear-gradient" +
                              gradientText2 +
                              "} ");

  let elementStyle = view._elementStyle;
  let rule = elementStyle.rules[1];

  // Initially the last property should be active.
  for (let i = 0; i < 3; ++i) {
    let prop = rule.textProps[i];
    is(prop.name, "background-image", "check the property name");
    is(prop.overridden, i !== 2, "check overridden for " + i);
  }

  yield togglePropStatus(view, rule.textProps[2]);

  // Now the first property should be active.
  for (let i = 0; i < 3; ++i) {
    let prop = rule.textProps[i];
    is(prop.overridden || !prop.enabled, i !== 0,
       "post-change check overridden for " + i);
  }
});
