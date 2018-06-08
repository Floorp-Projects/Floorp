/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(this, "ContentTaskUtils",
                               "resource://testing-common/ContentTaskUtils.jsm");

loader.lazyGetter(this, "WebExtensionInspectedWindowFront", () => {
  return require(
    "devtools/shared/fronts/addon/webextension-inspected-window"
  ).WebExtensionInspectedWindowFront;
}, true);

const SIDEBAR_ID = "an-extension-sidebar";
const SIDEBAR_TITLE = "Sidebar Title";

let extension;
let fakeExtCallerInfo;

let toolbox;
let inspector;

add_task(async function setupExtensionSidebar() {
  extension = ExtensionTestUtils.loadExtension({
    background() {
      // This is just an empty extension used to ensure that the caller extension uuid
      // actually exists.
    }
  });

  await extension.startup();

  fakeExtCallerInfo = {
    url: WebExtensionPolicy.getByID(extension.id).getURL("fake-caller-script.js"),
    lineNumber: 1,
    addonId: extension.id,
  };

  const res = await openInspectorForURL("about:blank");
  inspector = res.inspector;
  toolbox = res.toolbox;

  const onceSidebarCreated = toolbox.once(`extension-sidebar-created-${SIDEBAR_ID}`);
  toolbox.registerInspectorExtensionSidebar(SIDEBAR_ID, {title: SIDEBAR_TITLE});

  const sidebar = await onceSidebarCreated;

  // Test sidebar properties.
  is(sidebar, inspector.getPanel(SIDEBAR_ID),
     "Got an extension sidebar instance equal to the one saved in the inspector");
  is(sidebar.title, SIDEBAR_TITLE,
     "Got the expected title in the extension sidebar instance");
  is(sidebar.provider.props.title, SIDEBAR_TITLE,
     "Got the expeted title in the provider props");

  // Test sidebar Redux state.
  const inspectorStoreState = inspector.store.getState();
  ok("extensionsSidebar" in inspectorStoreState,
     "Got the extensionsSidebar sub-state in the inspector Redux store");
  Assert.deepEqual(inspectorStoreState.extensionsSidebar, {},
                   "The extensionsSidebar should be initially empty");
});

add_task(async function testSidebarSetObject() {
  const object = {
    propertyName: {
      nestedProperty: "propertyValue",
      anotherProperty: "anotherValue",
    },
  };

  const sidebar = inspector.getPanel(SIDEBAR_ID);
  sidebar.setObject(object);

  // Test updated sidebar Redux state.
  const inspectorStoreState = inspector.store.getState();
  is(Object.keys(inspectorStoreState.extensionsSidebar).length, 1,
     "The extensionsSidebar state contains the newly registered extension sidebar state");
  Assert.deepEqual(inspectorStoreState.extensionsSidebar, {
    [SIDEBAR_ID]: {
      viewMode: "object-treeview",
      object,
    },
  }, "Got the expected state for the registered extension sidebar");

  // Select the extension sidebar.
  const waitSidebarSelected = toolbox.once(`inspector-sidebar-select`);
  inspector.sidebar.show(SIDEBAR_ID);
  await waitSidebarSelected;

  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  // Test extension sidebar content.
  ok(sidebarPanelContent, "Got a sidebar panel for the registered extension sidebar");

  assertTreeView(sidebarPanelContent, {
    expectedTreeTables: 1,
    expectedStringCells: 2,
    expectedNumberCells: 0,
  });

  // Test sidebar refreshed on further sidebar.setObject calls.
  info("Change the inspected object in the extension sidebar object treeview");
  sidebar.setObject({aNewProperty: 123});

  assertTreeView(sidebarPanelContent, {
    expectedTreeTables: 1,
    expectedStringCells: 0,
    expectedNumberCells: 1,
  });
});

add_task(async function testSidebarSetObjectValueGrip() {
  const inspectedWindowFront = new WebExtensionInspectedWindowFront(
    toolbox.target.client, toolbox.target.form
  );

  const sidebar = inspector.getPanel(SIDEBAR_ID);
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  info("Testing sidebar.setObjectValueGrip with rootTitle");

  const expression = `
    var obj = Object.create(null);
    obj.prop1 = 123;
    obj[Symbol('sym1')] = 456;
    obj.cyclic = obj;
    obj;
  `;

  const evalResult = await inspectedWindowFront.eval(fakeExtCallerInfo, expression, {
    evalResultAsGrip: true,
    toolboxConsoleActorID: toolbox.target.form.consoleActor
  });

  sidebar.setObjectValueGrip(evalResult.valueGrip, "Expected Root Title");

  // Wait the ObjectInspector component to be rendered and test its content.
  await testSetExpressionSidebarPanel(sidebarPanelContent, {
    nodesLength: 4,
    propertiesNames: ["cyclic", "prop1", "Symbol(sym1)"],
    rootTitle: "Expected Root Title",
  });

  info("Testing sidebar.setObjectValueGrip without rootTitle");

  sidebar.setObjectValueGrip(evalResult.valueGrip);

  // Wait the ObjectInspector component to be rendered and test its content.
  await testSetExpressionSidebarPanel(sidebarPanelContent, {
    nodesLength: 4,
    propertiesNames: ["cyclic", "prop1", "Symbol(sym1)"],
  });

  inspectedWindowFront.destroy();
});

add_task(async function testSidebarDOMNodeHighlighting() {
  const inspectedWindowFront = new WebExtensionInspectedWindowFront(
    toolbox.target.client, toolbox.target.form
  );

  const sidebar = inspector.getPanel(SIDEBAR_ID);
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  const expression = "({ body: document.body })";

  const evalResult = await inspectedWindowFront.eval(fakeExtCallerInfo, expression, {
    evalResultAsGrip: true,
    toolboxConsoleActorID: toolbox.target.form.consoleActor
  });

  sidebar.setObjectValueGrip(evalResult.valueGrip);

  // Wait the DOM node to be rendered inside the component.
  await waitForObjectInspector(sidebarPanelContent, "node");

  // Wait for the object to be expanded so we only target the "body" property node, and
  // not the root object element.
  await ContentTaskUtils.waitForCondition(
    () => sidebarPanelContent.querySelectorAll(".object-inspector .tree-node").length > 1
  );

  // Get and verify the DOMNode and the "open inspector"" icon
  // rendered inside the ObjectInspector.
  assertObjectInspector(sidebarPanelContent, {
    expectedDOMNodes: 1,
    expectedOpenInspectors: 1,
  });

  // Test highlight DOMNode on mouseover.
  info("Highlight the node by moving the cursor on it");

  const onNodeHighlight = toolbox.once("node-highlight");

  moveMouseOnObjectInspectorDOMNode(sidebarPanelContent);

  const nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "body", "The correct node was highlighted");

  // Test unhighlight DOMNode on mousemove.
  info("Unhighlight the node by moving away from the node");
  const onNodeUnhighlight = toolbox.once("node-unhighlight");

  moveMouseOnPanelCenter(sidebarPanelContent);

  await onNodeUnhighlight;
  info("node-unhighlight event was fired when moving away from the node");

  inspectedWindowFront.destroy();
});

add_task(async function testSidebarDOMNodeOpenInspector() {
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  // Test DOMNode selected in the inspector when "open inspector"" icon clicked.
  info("Unselect node in the inspector");
  let onceNewNodeFront = inspector.selection.once("new-node-front");
  inspector.selection.setNodeFront(null);
  let nodeFront = await onceNewNodeFront;
  is(nodeFront, undefined, "The inspector selection should have been unselected");

  info("Select the ObjectInspector DOMNode in the inspector panel by clicking on it");

  // Once we click the open-inspector icon we expect a new node front to be selected
  // and the node to have been highlighted and unhighlighted.
  const onNodeHighlight = toolbox.once("node-highlight");
  const onNodeUnhighlight = toolbox.once("node-unhighlight");
  onceNewNodeFront = inspector.selection.once("new-node-front");

  clickOpenInspectorIcon(sidebarPanelContent);

  nodeFront = await onceNewNodeFront;
  is(nodeFront.displayName, "body", "The correct node has been selected");
  nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "body", "The correct node was highlighted");

  await onNodeUnhighlight;
});

add_task(async function teardownExtensionSidebar() {
  info("Remove the sidebar instance");

  toolbox.unregisterInspectorExtensionSidebar(SIDEBAR_ID);

  ok(!inspector.sidebar.getTabPanel(SIDEBAR_ID),
     "The rendered extension sidebar has been removed");

  const inspectorStoreState = inspector.store.getState();

  Assert.deepEqual(inspectorStoreState.extensionsSidebar, {},
                   "The extensions sidebar Redux store data has been cleared");

  await toolbox.destroy();

  await extension.unload();

  toolbox = null;
  inspector = null;
  extension = null;
});
