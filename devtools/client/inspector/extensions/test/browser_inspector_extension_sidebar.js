/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const {inspector} = await openInspectorForURL("about:blank");
  const {toolbox} = inspector;

  const sidebarId = "an-extension-sidebar";
  const sidebarTitle = "Sidebar Title";

  const waitSidebarCreated = toolbox.once(`extension-sidebar-created-${sidebarId}`);

  toolbox.registerInspectorExtensionSidebar(sidebarId, {title: sidebarTitle});

  const sidebar = await waitSidebarCreated;

  is(sidebar, inspector.getPanel(sidebarId),
     "Got an extension sidebar instance equal to the one saved in the inspector");

  is(sidebar.title, sidebarTitle,
     "Got the expected title in the extension sidebar instance");

  let inspectorStoreState = inspector.store.getState();

  ok("extensionsSidebar" in inspectorStoreState,
     "Got the extensionsSidebar sub-state in the inspector Redux store");

  Assert.deepEqual(inspectorStoreState.extensionsSidebar, {},
                   "The extensionsSidebar should be initially empty");

  let object = {
    propertyName: {
      nestedProperty: "propertyValue",
      anotherProperty: "anotherValue",
    },
  };

  sidebar.setObject(object);

  inspectorStoreState = inspector.store.getState();

  is(Object.keys(inspectorStoreState.extensionsSidebar).length, 1,
     "The extensionsSidebar state contains the newly registered extension sidebar state");

  Assert.deepEqual(inspectorStoreState.extensionsSidebar, {
    [sidebarId]: {
      viewMode: "object-treeview",
      object,
    },
  }, "Got the expected state for the registered extension sidebar");

  const waitSidebarSelected = toolbox.once(`inspector-sidebar-select`);

  inspector.sidebar.show(sidebarId);

  await waitSidebarSelected;

  const sidebarPanelContent = inspector.sidebar.getTabPanel(sidebarId);

  ok(sidebarPanelContent, "Got a sidebar panel for the registered extension sidebar");

  is(sidebarPanelContent.querySelectorAll("table.treeTable").length, 1,
     "The sidebar panel contains a rendered TreeView component");

  is(sidebarPanelContent.querySelectorAll("table.treeTable .stringCell").length, 2,
     "The TreeView component contains the expected number of string cells.");

  info("Change the inspected object in the extension sidebar object treeview");
  sidebar.setObject({aNewProperty: 123});

  is(sidebarPanelContent.querySelectorAll("table.treeTable .stringCell").length, 0,
     "The TreeView component doesn't contains any string cells anymore.");

  is(sidebarPanelContent.querySelectorAll("table.treeTable .numberCell").length, 1,
     "The TreeView component contains one number cells.");

  info("Remove the sidebar instance");

  toolbox.unregisterInspectorExtensionSidebar(sidebarId);

  ok(!inspector.sidebar.getTabPanel(sidebarId),
     "The rendered extension sidebar has been removed");

  inspectorStoreState = inspector.store.getState();

  Assert.deepEqual(inspectorStoreState.extensionsSidebar, {},
                   "The extensions sidebar Redux store data has been cleared");

  await toolbox.destroy();
});
