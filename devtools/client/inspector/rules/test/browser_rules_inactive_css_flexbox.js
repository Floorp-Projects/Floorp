/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test inactive flex properties.

const TEST_URI = `
<head>
  <style>
    #container {
      width: 200px;
      height: 100px;
      border: 1px solid #000;
      align-content: space-between;
      order: 1;
    }

    .flex-item {
      flex-basis: auto;
      flex-grow: 1;
      flex-shrink: 1;
      flex-direction: row;
    }

    #self-aligned {
      align-self: stretch;
    }
  </style>
</head>
<body>
    <h1>browser_rules_inactive_css_flexbox.js</h1>
    <div id="container" style="display:flex">
      <div class="flex-item item-1" style="order:1">1</div>
      <div class="flex-item item-2" style="order:2">2</div>
      <div class="flex-item item-3" style="order:3">3</div>
    </div>
    <div id="self-aligned"></div>
</body>`;

const BEFORE = [
  {
    selector: "#self-aligned",
    inactiveDeclarations: [
      {
        declaration: {
          "align-self": "stretch",
        },
        ruleIndex: 1,
      },
    ],
  },
  {
    selector: ".item-2",
    activeDeclarations: [
      {
        declarations: {
          "order": "2",
        },
        ruleIndex: 0,
      },
      {
        declarations: {
          "flex-basis": "auto",
          "flex-grow": "1",
          "flex-shrink": "1",
        },
        ruleIndex: 1,
      },
    ],
    inactiveDeclarations: [
      {
        declaration: {
          "flex-direction": "row",
        },
        ruleIndex: 1,
      },
    ],
  },
  {
    selector: "#container",
    activeDeclarations: [
      {
        declarations: {
          "display": "flex",
        },
        ruleIndex: 0,
      },
      {
        declarations: {
          width: "200px",
          height: "100px",
          border: "1px solid #000",
          "align-content": "space-between",
        },
        ruleIndex: 1,
      },
    ],
    inactiveDeclarations: [
      {
        declaration: {
          "order": "1",
        },
        ruleIndex: 1,
      },
    ],
  },
];

const AFTER = [
  {
    selector: ".item-2",
    inactiveDeclarations: [
      {
        declaration: {
          "order": "2",
        },
        ruleIndex: 0,
      },
      {
        declaration: {
          "flex-basis": "auto",
        },
        ruleIndex: 1,
      },
      {
        declaration: {
          "flex-grow": "1",
        },
        ruleIndex: 1,
      },
      {
        declaration: {
          "flex-shrink": "1",
        },
        ruleIndex: 1,
      },
      {
        declaration: {
          "flex-direction": "row",
        },
        ruleIndex: 1,
      },
    ],
  },
];

add_task(async function() {
  await pushPref("devtools.inspector.inactive.css.enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await runInactiveCSSTests(view, inspector, BEFORE);

  // Toggle `display:flex` to disabled.
  await toggleDeclaration(inspector, view, 0, {
    display: "flex",
  });
  await runInactiveCSSTests(view, inspector, AFTER);
});
