/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test inactive grid properties.

const TEST_URI = `
<head>
  <style>
    #container {
      width: 200px;
      height: 100px;
      border: 1px solid #000;
      column-gap: 10px;
      row-gap: 10px;
      align-self: start;
    }

    .item-1 {
      grid-column-start: 1;
      grid-column-end: 3;
      grid-row-start: 1;
      grid-row-end: auto;
      flex-direction: row
    }

    #self-aligned {
      align-self: stretch;
    }
  </style>
</head>
<body>
    <h1>browser_rules_inactive_css_grid.js</h1>
    <div id="container" style="display:grid">
      <div class="grid-item item-1">1</div>
      <div class="grid-item item-2">2</div>
      <div class="grid-item item-3">3</div>
      <div class="grid-item item-4">4</div>
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
    selector: ".item-1",
    activeDeclarations: [
      {
        declarations: {
          "grid-column-start": "1",
          "grid-column-end": "3",
          "grid-row-start": "1",
          "grid-row-end": "auto",
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
          display: "grid",
        },
        ruleIndex: 0,
      },
      {
        declarations: {
          width: "200px",
          height: "100px",
          border: "1px solid #000",
          "column-gap": "10px",
          "row-gap": "10px",
        },
        ruleIndex: 1,
      },
    ],
    inactiveDeclarations: [
      {
        declaration: {
          "align-self": "start",
        },
        ruleIndex: 1,
      },
    ],
  },
];

const AFTER = [
  {
    activeDeclarations: [
      {
        declarations: {
          display: "grid",
        },
        ruleIndex: 0,
      },
      {
        declarations: {
          width: "200px",
          height: "100px",
          border: "1px solid #000",
        },
        ruleIndex: 1,
      },
    ],
    inactiveDeclarations: [
      {
        declaration: {
          "column-gap": "10px",
        },
        ruleIndex: 1,
      },
      {
        declaration: {
          "row-gap": "10px",
        },
        ruleIndex: 1,
      },
      {
        declaration: {
          "align-self": "start",
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

  // Toggle `display:grid` to disabled.
  await toggleDeclaration(inspector, view, 0, {
    display: "grid",
  });
  await view.once("ruleview-refreshed");
  await runInactiveCSSTests(view, inspector, AFTER);
});
