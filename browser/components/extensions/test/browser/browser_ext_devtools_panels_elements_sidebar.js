/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals getExtensionSidebarActors, expectNoSuchActorIDs, testSetExpressionSidebarPanel */

// Import the shared test helpers from the related devtools tests.
loadTestSubscript("head_devtools.js");
loadTestSubscript("head_devtools_inspector_sidebar.js");

function isActiveSidebarTabTitle(inspector, expectedTabTitle, message) {
  const actualTabTitle = inspector.panelDoc.querySelector(
    "#inspector-sidebar .tabs-menu-item.is-active"
  ).innerText;
  is(actualTabTitle, expectedTabTitle, message);
}

function testSetObjectSidebarPanel(panel, expectedCellType, expectedTitle) {
  is(
    panel.querySelectorAll("table.treeTable").length,
    1,
    "The sidebar panel contains a rendered TreeView component"
  );

  is(
    panel.querySelectorAll(`table.treeTable .${expectedCellType}Cell`).length,
    1,
    `The TreeView component contains the expected a cell of type ${expectedCellType}`
  );

  if (expectedTitle) {
    const panelTree = panel.querySelector("table.treeTable");
    ok(
      panelTree.innerText.includes(expectedTitle),
      "The optional root object title has been included in the object tree"
    );
  }
}

async function testSidebarPanelSelect(extension, inspector, tabId, expected) {
  const { sidebarShown, sidebarHidden, activeSidebarTabTitle } = expected;

  inspector.sidebar.show(tabId);

  const shown = await extension.awaitMessage("devtools_sidebar_shown");
  is(
    shown,
    sidebarShown,
    "Got the shown event on the second extension sidebar"
  );

  if (sidebarHidden) {
    const hidden = await extension.awaitMessage("devtools_sidebar_hidden");
    is(
      hidden,
      sidebarHidden,
      "Got the hidden event on the first extension sidebar"
    );
  }

  isActiveSidebarTabTitle(
    inspector,
    activeSidebarTabTitle,
    "Got the expected title on the active sidebar tab"
  );
}

add_task(async function test_devtools_panels_elements_sidebar() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  async function devtools_page() {
    const sidebar1 = await browser.devtools.panels.elements.createSidebarPane(
      "Test Sidebar 1"
    );
    const sidebar2 = await browser.devtools.panels.elements.createSidebarPane(
      "Test Sidebar 2"
    );
    const sidebar3 = await browser.devtools.panels.elements.createSidebarPane(
      "Test Sidebar 3"
    );
    const sidebar4 = await browser.devtools.panels.elements.createSidebarPane(
      "Test Sidebar 4"
    );

    const onShownListener = (event, sidebarInstance) => {
      browser.test.sendMessage(`devtools_sidebar_${event}`, sidebarInstance);
    };

    sidebar1.onShown.addListener(() => onShownListener("shown", "sidebar1"));
    sidebar2.onShown.addListener(() => onShownListener("shown", "sidebar2"));
    sidebar3.onShown.addListener(() => onShownListener("shown", "sidebar3"));
    sidebar4.onShown.addListener(() => onShownListener("shown", "sidebar4"));

    sidebar1.onHidden.addListener(() => onShownListener("hidden", "sidebar1"));
    sidebar2.onHidden.addListener(() => onShownListener("hidden", "sidebar2"));
    sidebar3.onHidden.addListener(() => onShownListener("hidden", "sidebar3"));
    sidebar4.onHidden.addListener(() => onShownListener("hidden", "sidebar4"));

    // Refresh the sidebar content on every inspector selection.
    browser.devtools.panels.elements.onSelectionChanged.addListener(() => {
      const expression = `
        var obj = Object.create(null);
        obj.prop1 = 123;
        obj[Symbol('sym1')] = 456;
        obj.cyclic = obj;
        obj;
      `;
      sidebar1.setExpression(expression, "sidebar.setExpression rootTitle");
    });

    sidebar2.setObject({ anotherPropertyName: 123 });
    sidebar3.setObject(
      { propertyName: "propertyValue" },
      "Optional Root Object Title"
    );

    sidebar4.setPage("sidebar.html");

    browser.test.sendMessage("devtools_page_loaded");
  }

  function sidebar() {
    browser.test.sendMessage("sidebar-loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         <script src="devtools_page.js"></script>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
      "sidebar.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         sidebar panel
         <script src="sidebar.js"></script>
       </body>
      </html>`,
      "sidebar.js": sidebar,
    },
  });

  await extension.startup();

  const toolbox = await openToolboxForTab(tab);

  await extension.awaitMessage("devtools_page_loaded");

  const waitInspector = toolbox.once("inspector-selected");
  toolbox.selectTool("inspector");
  await waitInspector;

  const sidebarIds = Array.from(toolbox._inspectorExtensionSidebars.keys());

  const inspector = await toolbox.getPanel("inspector");

  info("Test extension inspector sidebar 1 (sidebar.setExpression)");

  inspector.sidebar.show(sidebarIds[0]);

  const shownSidebarInstance = await extension.awaitMessage(
    "devtools_sidebar_shown"
  );

  is(
    shownSidebarInstance,
    "sidebar1",
    "Got the shown event on the first extension sidebar"
  );

  isActiveSidebarTabTitle(
    inspector,
    "Test Sidebar 1",
    "Got the expected title on the active sidebar tab"
  );

  const sidebarPanel1 = inspector.sidebar.getTabPanel(sidebarIds[0]);

  ok(
    sidebarPanel1,
    "Got a rendered sidebar panel for the first registered extension sidebar"
  );

  info("Waiting for the first panel to be rendered");

  // Verify that the panel contains an ObjectInspector, with the expected number of nodes
  // and with the expected property names.
  await testSetExpressionSidebarPanel(sidebarPanel1, {
    nodesLength: 4,
    propertiesNames: ["cyclic", "prop1", "Symbol(sym1)"],
    rootTitle: "sidebar.setExpression rootTitle",
  });

  // Retrieve the actors currently rendered into the extension sidebars.
  const actors = getExtensionSidebarActors(inspector);

  info(
    "Test extension inspector sidebar 2 (sidebar.setObject without a root title)"
  );

  await testSidebarPanelSelect(extension, inspector, sidebarIds[1], {
    sidebarShown: "sidebar2",
    sidebarHidden: "sidebar1",
    activeSidebarTabTitle: "Test Sidebar 2",
  });

  const sidebarPanel2 = inspector.sidebar.getTabPanel(sidebarIds[1]);

  ok(
    sidebarPanel2,
    "Got a rendered sidebar panel for the second registered extension sidebar"
  );

  testSetObjectSidebarPanel(sidebarPanel2, "number");

  info(
    "Test extension inspector sidebar 3 (sidebar.setObject with a root title)"
  );

  await testSidebarPanelSelect(extension, inspector, sidebarIds[2], {
    sidebarShown: "sidebar3",
    sidebarHidden: "sidebar2",
    activeSidebarTabTitle: "Test Sidebar 3",
  });

  const sidebarPanel3 = inspector.sidebar.getTabPanel(sidebarIds[2]);

  ok(
    sidebarPanel3,
    "Got a rendered sidebar panel for the third registered extension sidebar"
  );

  testSetObjectSidebarPanel(
    sidebarPanel3,
    "string",
    "Optional Root Object Title"
  );

  info(
    "Unloading the extension and check that all the sidebar have been removed"
  );

  inspector.sidebar.show(sidebarIds[3]);

  const shownSidebarInstance4 = await extension.awaitMessage(
    "devtools_sidebar_shown"
  );
  const hiddenSidebarInstance3 = await extension.awaitMessage(
    "devtools_sidebar_hidden"
  );

  is(
    shownSidebarInstance4,
    "sidebar4",
    "Got the shown event on the third extension sidebar"
  );
  is(
    hiddenSidebarInstance3,
    "sidebar3",
    "Got the hidden event on the second extension sidebar"
  );

  isActiveSidebarTabTitle(
    inspector,
    "Test Sidebar 4",
    "Got the expected title on the active sidebar tab"
  );

  await extension.awaitMessage("sidebar-loaded");

  await extension.unload();

  is(
    Array.from(toolbox._inspectorExtensionSidebars.keys()).length,
    0,
    "All the registered sidebars have been unregistered on extension unload"
  );

  is(
    inspector.sidebar.getTabPanel(sidebarIds[0]),
    null,
    "The first registered sidebar has been removed"
  );

  is(
    inspector.sidebar.getTabPanel(sidebarIds[1]),
    null,
    "The second registered sidebar has been removed"
  );

  is(
    inspector.sidebar.getTabPanel(sidebarIds[2]),
    null,
    "The third registered sidebar has been removed"
  );

  is(
    inspector.sidebar.getTabPanel(sidebarIds[3]),
    null,
    "The fourth registered sidebar has been removed"
  );

  await expectNoSuchActorIDs(toolbox.target.client, actors);

  await closeToolboxForTab(tab);

  BrowserTestUtils.removeTab(tab);
});
