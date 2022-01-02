/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test inactive grid properties.

const TEST_URI = `
<head>
  <style>
    html {
      grid-area: foo;
    }
    #container {
      width: 200px;
      height: 100px;
      border: 1px solid #000;
      column-gap: 10px;
      row-gap: 10px;
      align-self: start;
      position: relative;
    }

    .item-1 {
      grid-column-start: 1;
      grid-column-end: 3;
      grid-row-start: 1;
      grid-row-end: auto;
      flex-direction: row
    }

    #abspos {
      position: absolute;
      grid-column: 2;
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
      <div class="grid-item item-5">
        <div id="abspos">AbsPos item</div>
      </div>
    </div>
    <div id="self-aligned"></div>
</body>`;

const BEFORE = [
  {
    // Check first that the getting grid-related data about the <html> node doesn't break.
    // See bug 1576484.
    selector: "html",
    inactiveDeclarations: [
      {
        declaration: {
          "grid-area": "foo",
        },
        ruleIndex: 1,
      },
    ],
  },
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
    selector: "#abspos",
    activeDeclarations: [
      {
        declarations: {
          "grid-column": 2,
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
  {
    selector: "#abspos",
    inactiveDeclarations: [
      {
        declaration: {
          "grid-column": 2,
        },
        ruleIndex: 1,
      },
    ],
  },
];

add_task(async function() {
  await pushPref("devtools.inspector.inactive.css.enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await runInactiveCSSTests(view, inspector, BEFORE);

  // Toggle `display:grid` to disabled.
  await toggleDeclaration(view, 0, {
    display: "grid",
  });
  await view.once("ruleview-refreshed");
  await runInactiveCSSTests(view, inspector, AFTER);

  info("Toggle `display: grid` to enabled again.");
  await selectNode("#container", inspector);
  await toggleDeclaration(view, 0, {
    display: "grid",
  });
  await runAbsPosGridElementTests(view, inspector);
});

/**
 * Tests for absolute positioned elements in a grid.
 */
async function runAbsPosGridElementTests(view, inspector) {
  info("Toggling `position: relative` to disabled.");
  await toggleDeclaration(view, 1, {
    position: "relative",
  });
  await runInactiveCSSTests(view, inspector, [
    {
      selector: "#abspos",
      inactiveDeclarations: [
        {
          declaration: {
            "grid-column": 2,
          },
          ruleIndex: 1,
        },
      ],
    },
  ]);

  info("Toggling `position: relative` back to enabled.");
  await selectNode("#container", inspector);
  await toggleDeclaration(view, 1, {
    position: "relative",
  });

  info("Toggling `position: absolute` on grid element to disabled.");
  await selectNode("#abspos", inspector);
  await toggleDeclaration(view, 1, {
    position: "absolute",
  });

  await runInactiveCSSTests(view, inspector, [
    {
      selector: "#abspos",
      inactiveDeclarations: [
        {
          declaration: {
            "grid-column": 2,
          },
          ruleIndex: 1,
        },
      ],
    },
  ]);
}
