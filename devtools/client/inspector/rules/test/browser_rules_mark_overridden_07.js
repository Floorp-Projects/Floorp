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
      line-height: 100px !important;
    }

    span {
      font-size: 12px;
      line-height: 10px;
    }
  </style>
  <body>
    <span>
      <div id='testid' class='testclass'>Styled Node</div>
    </span>
  </body>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  await testMarkOverridden(inspector, view);
});

function testMarkOverridden(inspector, view) {
  const elementStyle = view._elementStyle;

  const RESULTS = [
    // We skip the first element
    [],
    [{ name: "margin-left", value: "23px", overridden: true }],
    [
      { name: "margin-right", value: "23px", overridden: false },
      { name: "margin-left", value: "1px", overridden: false },
    ],
    [
      { name: "font-size", value: "12px", overridden: false },
      { name: "line-height", value: "10px", overridden: false },
    ],
    [
      { name: "margin-right", value: "1px", overridden: true },
      { name: "font-size", value: "79px", overridden: true },
      { name: "line-height", value: "100px", overridden: true },
    ],
  ];

  for (let i = 1; i < RESULTS.length; ++i) {
    const idRule = elementStyle.rules[i];

    for (const propIndex in RESULTS[i]) {
      const expected = RESULTS[i][propIndex];
      const prop = idRule.textProps[propIndex];

      info("Checking rule " + i + ", property " + propIndex);

      is(prop.name, expected.name, "check property name");
      is(prop.value, expected.value, "check property value");
      is(prop.overridden, expected.overridden, "check property overridden");
    }
  }
}
