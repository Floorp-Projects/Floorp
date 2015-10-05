/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// specificity of the rule.

const TEST_URI = `
  <style type='text/css'>
    #testid {
      margin-left: 23px;
    }

    div {
      margin-right: 23px;
      margin-left: 1px !important;
    }

    body {
      margin-right: 1px !important;
      font-size: 79px;
    }

    span {
      font-size: 12px;
    }
  </style>
  <body>
    <span>
      <div id='testid' class='testclass'>Styled Node</div>
    </span>
  </body>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testMarkOverridden(inspector, view);
});

function* testMarkOverridden(inspector, view) {
  let elementStyle = view._elementStyle;

  let RESULTS = [
    // We skip the first element
    [],
    [{name: "margin-left", value: "23px", overridden: true}],
    [{name: "margin-right", value: "23px", overridden: false},
     {name: "margin-left", value: "1px", overridden: false}],
    [{name: "font-size", value: "12px", overridden: false}],
    [{name: "font-size", value: "79px", overridden: true}]
  ];

  for (let i = 1; i < RESULTS.length; ++i) {
    let idRule = elementStyle.rules[i];

    for (let propIndex in RESULTS[i]) {
      let expected = RESULTS[i][propIndex];
      let prop = idRule.textProps[propIndex];

      info("Checking rule " + i + ", property " + propIndex);

      is(prop.name, expected.name, "check property name");
      is(prop.value, expected.value, "check property value");
      is(prop.overridden, expected.overridden, "check property overridden");
    }
  }
}
