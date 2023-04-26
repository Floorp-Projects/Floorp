/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test inactive css properties in XUL documents.

const TEST_URI = URL_ROOT_SSL + "doc_inactive_css_xul.xhtml";

const TEST_DATA = [
  {
    selector: "#test-img-in-xul",
    inactiveDeclarations: [
      {
        declaration: { "grid-column-gap": "5px" },
        ruleIndex: 0,
      },
    ],
    activeDeclarations: [
      {
        declarations: {
          width: "10px",
          height: "10px",
        },
        ruleIndex: 0,
      },
    ],
  },
];

add_task(async () => {
  await SpecialPowers.pushPermissions([
    { type: "allowXULXBL", allow: true, context: URL_ROOT_SSL },
  ]);

  info("Open a url to a XUL document");
  await addTab(TEST_URI);

  info("Open the inspector");
  const { inspector, view } = await openRuleView();

  await runInactiveCSSTests(view, inspector, TEST_DATA);
});
