/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource://devtools/client/framework/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://devtools/shared/Loader.jsm");

add_task(async function test_devtools_panels_elements_sidebar() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  async function devtools_page() {
    const sidebar1 = await browser.devtools.panels.elements.createSidebarPane("Test Sidebar 1");
    const sidebar2 = await browser.devtools.panels.elements.createSidebarPane("Test Sidebar 2");

    const onShownListener = (event, sidebarInstance) => {
      browser.test.sendMessage(`devtools_sidebar_${event}`, sidebarInstance);
    };

    sidebar1.onShown.addListener(() => onShownListener("shown", "sidebar1"));
    sidebar2.onShown.addListener(() => onShownListener("shown", "sidebar2"));
    sidebar1.onHidden.addListener(() => onShownListener("hidden", "sidebar1"));
    sidebar2.onHidden.addListener(() => onShownListener("hidden", "sidebar2"));

    sidebar1.setObject({propertyName: "propertyValue"}, "Optional Root Object Title");
    sidebar2.setObject({anotherPropertyName: 123});

    browser.test.sendMessage("devtools_page_loaded");
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
    },
  });

  await extension.startup();

  let target = devtools.TargetFactory.forTab(tab);

  const toolbox = await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  await extension.awaitMessage("devtools_page_loaded");

  const waitInspector = toolbox.once("inspector-selected");
  toolbox.selectTool("inspector");
  await waitInspector;

  const sidebarIds = Array.from(toolbox._inspectorExtensionSidebars.keys());

  const inspector = await toolbox.getPanel("inspector");

  inspector.sidebar.show(sidebarIds[0]);

  const shownSidebarInstance = await extension.awaitMessage("devtools_sidebar_shown");

  is(shownSidebarInstance, "sidebar1", "Got the shown event on the first extension sidebar");

  const sidebarPanel1 = inspector.sidebar.getTabPanel(sidebarIds[0]);

  ok(sidebarPanel1, "Got a rendered sidebar panel for the first registered extension sidebar");

  is(sidebarPanel1.querySelectorAll("table.treeTable").length, 1,
     "The first sidebar panel contains a rendered TreeView component");

  is(sidebarPanel1.querySelectorAll("table.treeTable .stringCell").length, 1,
     "The TreeView component contains the expected number of string cells.");

  const sidebarPanel1Tree = sidebarPanel1.querySelector("table.treeTable");
  ok(
    sidebarPanel1Tree.innerText.includes("Optional Root Object Title"),
    "The optional root object title has been included in the object tree"
  );

  inspector.sidebar.show(sidebarIds[1]);

  const shownSidebarInstance2 = await extension.awaitMessage("devtools_sidebar_shown");
  const hiddenSidebarInstance1 = await extension.awaitMessage("devtools_sidebar_hidden");

  is(shownSidebarInstance2, "sidebar2", "Got the shown event on the second extension sidebar");
  is(hiddenSidebarInstance1, "sidebar1", "Got the hidden event on the first extension sidebar");

  const sidebarPanel2 = inspector.sidebar.getTabPanel(sidebarIds[1]);

  ok(sidebarPanel2, "Got a rendered sidebar panel for the second registered extension sidebar");

  is(sidebarPanel2.querySelectorAll("table.treeTable").length, 1,
     "The second sidebar panel contains a rendered TreeView component");

  is(sidebarPanel2.querySelectorAll("table.treeTable .numberCell").length, 1,
     "The TreeView component contains the expected a cell of type number.");

  await extension.unload();

  is(Array.from(toolbox._inspectorExtensionSidebars.keys()).length, 0,
     "All the registered sidebars have been unregistered on extension unload");

  is(inspector.sidebar.getTabPanel(sidebarIds[0]), undefined,
     "The first registered sidebar has been removed");

  is(inspector.sidebar.getTabPanel(sidebarIds[1]), undefined,
     "The second registered sidebar has been removed");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await BrowserTestUtils.removeTab(tab);
});
