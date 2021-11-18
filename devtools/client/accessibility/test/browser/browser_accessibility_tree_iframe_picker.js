/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the accessibility panel works as expected when using the iframe picker

const TEST_URI = `data:text/html,<meta charset=utf8>
  <head>
    <title>TopLevel</title>
    <style>h1 { color: lightgrey; }</style>
  </head>
  <body>
    <h1>Top level header</h1>
    <p>This is a paragraph.</p>
    <iframe src="data:text/html,<meta charset=utf8>
      <head>
        <title>iframe</title>
        <style>h2 { color: aliceblue }</style>
      </head>
      <body>
        <h2>Iframe header</h2>

        <iframe src='data:text/html,<meta charset=utf8>
          <head>
            <title>nested iframe</title>
            <style>h2 { color: lightpink }</style>
          </head>
          <body>
            <h2>Nested Iframe header</h2>
          </body>
        '></iframe>

      </body>
    "></iframe>`;

add_task(async () => {
  const env = await addTestTab(TEST_URI);
  const { doc, toolbox, win } = env;

  await checkTree(env, [
    {
      role: "document",
      name: `"TopLevel"`,
    },
  ]);

  info("Select the iframe in the iframe picker");
  // Get the iframe picker items
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));

  let onInitialized = win.once(win.EVENTS.INITIALIZED);
  frames[1].click();
  await onInitialized;

  await checkTree(env, [
    {
      role: "document",
      name: `"iframe"`,
    },
  ]);

  info(
    "Run a constrast audit to check only issues from selected iframe tree are displayed"
  );
  const CONTRAST_MENU_ITEM_INDEX = 2;
  const onUpdated = win.once(win.EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED);
  await toggleMenuItem(
    doc,
    toolbox.doc,
    TREE_FILTERS_MENU_ID,
    CONTRAST_MENU_ITEM_INDEX
  );
  await onUpdated;
  // wait until the tree is filtered (i.e. the audit is done and only nodes with issues
  // should be displayed)
  await waitFor(() => doc.querySelector(".treeTable.filtered"));

  await checkTree(env, [
    {
      role: "text leaf",
      name: `"Iframe header"contrast`,
      badges: ["contrast"],
      level: 1,
    },
    {
      role: "text leaf",
      name: `"Nested Iframe header"contrast`,
      badges: ["contrast"],
      level: 1,
    },
  ]);

  info("Select the top level document back");
  onInitialized = win.once(win.EVENTS.INITIALIZED);
  frames[0].click();
  await onInitialized;

  await checkTree(env, [
    {
      role: "document",
      name: `"TopLevel"`,
    },
  ]);

  await closeTabToolboxAccessibility(env.tab);
});

function checkTree(env, tree) {
  return runA11yPanelTests(
    [
      {
        expected: {
          tree,
        },
      },
    ],
    env
  );
}
