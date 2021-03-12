/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ContentTaskUtils",
  "resource://testing-common/ContentTaskUtils.jsm"
);

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
    },
  });

  await extension.startup();

  fakeExtCallerInfo = {
    url: WebExtensionPolicy.getByID(extension.id).getURL(
      "fake-caller-script.js"
    ),
    lineNumber: 1,
    addonId: extension.id,
  };

  const res = await openInspectorForURL("about:blank");
  inspector = res.inspector;
  toolbox = res.toolbox;

  const onceSidebarCreated = toolbox.once(
    `extension-sidebar-created-${SIDEBAR_ID}`
  );
  toolbox.registerInspectorExtensionSidebar(SIDEBAR_ID, {
    title: SIDEBAR_TITLE,
  });

  const sidebar = await onceSidebarCreated;

  // Test sidebar properties.
  is(
    sidebar,
    inspector.getPanel(SIDEBAR_ID),
    "Got an extension sidebar instance equal to the one saved in the inspector"
  );
  is(
    sidebar.title,
    SIDEBAR_TITLE,
    "Got the expected title in the extension sidebar instance"
  );
  is(
    sidebar.provider.props.title,
    SIDEBAR_TITLE,
    "Got the expeted title in the provider props"
  );

  // Test sidebar Redux state.
  const inspectorStoreState = inspector.store.getState();
  ok(
    "extensionsSidebar" in inspectorStoreState,
    "Got the extensionsSidebar sub-state in the inspector Redux store"
  );
  Assert.deepEqual(
    inspectorStoreState.extensionsSidebar,
    {},
    "The extensionsSidebar should be initially empty"
  );
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
  is(
    Object.keys(inspectorStoreState.extensionsSidebar).length,
    1,
    "The extensionsSidebar state contains the newly registered extension sidebar state"
  );
  Assert.deepEqual(
    inspectorStoreState.extensionsSidebar,
    {
      [SIDEBAR_ID]: {
        viewMode: "object-treeview",
        object,
      },
    },
    "Got the expected state for the registered extension sidebar"
  );

  // Select the extension sidebar.
  const waitSidebarSelected = toolbox.once(`inspector-sidebar-select`);
  inspector.sidebar.show(SIDEBAR_ID);
  await waitSidebarSelected;

  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  // Test extension sidebar content.
  ok(
    sidebarPanelContent,
    "Got a sidebar panel for the registered extension sidebar"
  );

  assertTreeView(sidebarPanelContent, {
    expectedTreeTables: 1,
    expectedStringCells: 2,
    expectedNumberCells: 0,
  });

  // Test sidebar refreshed on further sidebar.setObject calls.
  info("Change the inspected object in the extension sidebar object treeview");
  sidebar.setObject({ aNewProperty: 123 });

  assertTreeView(sidebarPanelContent, {
    expectedTreeTables: 1,
    expectedStringCells: 0,
    expectedNumberCells: 1,
  });
});

add_task(async function testSidebarSetExpressionResult() {
  const inspectedWindowFront = await toolbox.target.getFront(
    "webExtensionInspectedWindow"
  );
  const sidebar = inspector.getPanel(SIDEBAR_ID);
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  info("Testing sidebar.setExpressionResult with rootTitle");

  const expression = `
    var obj = Object.create(null);
    obj.prop1 = 123;
    obj[Symbol('sym1')] = 456;
    obj.cyclic = obj;
    obj;
  `;

  const consoleFront = await toolbox.target.getFront("console");
  let evalResult = await inspectedWindowFront.eval(
    fakeExtCallerInfo,
    expression,
    {
      consoleFront,
    }
  );

  sidebar.setExpressionResult(evalResult, "Expected Root Title");

  // Wait the ObjectInspector component to be rendered and test its content.
  await testSetExpressionSidebarPanel(sidebarPanelContent, {
    nodesLength: 4,
    propertiesNames: ["cyclic", "prop1", "Symbol(sym1)"],
    rootTitle: "Expected Root Title",
  });

  info("Testing sidebar.setExpressionResult without rootTitle");

  sidebar.setExpressionResult(evalResult);

  // Wait the ObjectInspector component to be rendered and test its content.
  await testSetExpressionSidebarPanel(sidebarPanelContent, {
    nodesLength: 4,
    propertiesNames: ["cyclic", "prop1", "Symbol(sym1)"],
  });

  info("Test expanding the object");
  const oi = sidebarPanelContent.querySelector(".tree");
  const cyclicNode = oi.querySelectorAll(".node")[1];
  ok(cyclicNode.innerText.includes("cyclic"), "Found the expected node");
  cyclicNode.click();

  await ContentTaskUtils.waitForCondition(
    () => oi.querySelectorAll(".node").length === 7,
    "Wait for the 'cyclic' node to be expanded"
  );

  await ContentTaskUtils.waitForCondition(
    () => oi.querySelector(".tree-node.focused"),
    "Wait for the 'cyclic' node to be focused"
  );
  ok(
    oi.querySelector(".tree-node.focused").innerText.includes("cyclic"),
    "'cyclic' node is focused"
  );

  info("Test keyboard navigation");
  EventUtils.synthesizeKey("KEY_ArrowLeft", {}, oi.ownerDocument.defaultView);
  await ContentTaskUtils.waitForCondition(
    () => oi.querySelectorAll(".node").length === 4,
    "Wait for the 'cyclic' node to be collapsed"
  );
  ok(
    oi.querySelector(".tree-node.focused").innerText.includes("cyclic"),
    "'cyclic' node is still focused"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, oi.ownerDocument.defaultView);
  await ContentTaskUtils.waitForCondition(
    () => oi.querySelectorAll(".tree-node")[2].classList.contains("focused"),
    "Wait for the 'prop1' node to be focused"
  );

  ok(
    oi.querySelector(".tree-node.focused").innerText.includes("prop1"),
    "'prop1' node is focused"
  );

  info(
    "Testing sidebar.setExpressionResult for an expression returning a longstring"
  );
  evalResult = await inspectedWindowFront.eval(
    fakeExtCallerInfo,
    `"ab ".repeat(10000)`,
    {
      consoleFront,
    }
  );
  sidebar.setExpressionResult(evalResult);

  await ContentTaskUtils.waitForCondition(() => {
    const longStringEl = sidebarPanelContent.querySelector(
      ".tree .objectBox-string"
    );
    return (
      longStringEl && longStringEl.textContent.includes("ab ".repeat(10000))
    );
  }, "Wait for the longString to be render with its full text");
  ok(true, "The longString is expanded and its full text is displayed");

  info(
    "Testing sidebar.setExpressionResult for an expression returning a primitive"
  );
  evalResult = await inspectedWindowFront.eval(fakeExtCallerInfo, `1 + 2`, {
    consoleFront,
  });
  sidebar.setExpressionResult(evalResult);
  const numberEl = await ContentTaskUtils.waitForCondition(
    () => sidebarPanelContent.querySelector(".objectBox-number"),
    "Wait for the result number element to be rendered"
  );
  is(numberEl.textContent, "3", `The "1 + 2" expression was evaluated as "3"`);

  inspectedWindowFront.destroy();
});

add_task(async function testSidebarDOMNodeHighlighting() {
  const inspectedWindowFront = await toolbox.target.getFront(
    "webExtensionInspectedWindow"
  );
  const sidebar = inspector.getPanel(SIDEBAR_ID);
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  const expression = "({ body: document.body })";

  const consoleFront = await toolbox.target.getFront("console");
  const evalResult = await inspectedWindowFront.eval(
    fakeExtCallerInfo,
    expression,
    {
      consoleFront,
    }
  );

  sidebar.setExpressionResult(evalResult);

  // Wait the DOM node to be rendered inside the component.
  await waitForObjectInspector(sidebarPanelContent, "node");

  // Wait for the object to be expanded so we only target the "body" property node, and
  // not the root object element.
  await ContentTaskUtils.waitForCondition(
    () =>
      sidebarPanelContent.querySelectorAll(".object-inspector .tree-node")
        .length > 1
  );

  // Get and verify the DOMNode and the "open inspector"" icon
  // rendered inside the ObjectInspector.

  assertObjectInspector(sidebarPanelContent, {
    expectedDOMNodes: 2,
    expectedOpenInspectors: 2,
  });

  // Test highlight DOMNode on mouseover.
  info("Highlight the node by moving the cursor on it");

  const onNodeHighlight = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );

  moveMouseOnObjectInspectorDOMNode(sidebarPanelContent);

  const { nodeFront } = await onNodeHighlight;
  is(nodeFront.displayName, "body", "The correct node was highlighted");

  // Test unhighlight DOMNode on mousemove.
  info("Unhighlight the node by moving away from the node");
  const onNodeUnhighlight = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );

  moveMouseOnPanelCenter(sidebarPanelContent);

  await onNodeUnhighlight;
  info("The node is no longer highlighted");

  inspectedWindowFront.destroy();
});

add_task(async function testSidebarDOMNodeOpenInspector() {
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  // Test DOMNode selected in the inspector when "open inspector"" icon clicked.
  info("Unselect node in the inspector");
  let onceNewNodeFront = inspector.selection.once("new-node-front");
  inspector.selection.setNodeFront(null);
  let nodeFront = await onceNewNodeFront;
  is(nodeFront, null, "The inspector selection should have been unselected");

  info(
    "Select the ObjectInspector DOMNode in the inspector panel by clicking on it"
  );

  // In test mode, shown highlighters are not automatically hidden after a delay to
  // prevent intermittent test failures from race conditions.
  // Restore this behavior just for this test because it is explicitly checked.
  const HIGHLIGHTER_AUTOHIDE_TIMER = inspector.HIGHLIGHTER_AUTOHIDE_TIMER;
  inspector.HIGHLIGHTER_AUTOHIDE_TIMER = 1000;
  registerCleanupFunction(() => {
    // Restore the value to disable autohiding to not impact other tests.
    inspector.HIGHLIGHTER_AUTOHIDE_TIMER = HIGHLIGHTER_AUTOHIDE_TIMER;
  });

  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  // Once we click the open-inspector icon we expect a new node front to be selected
  // and the node to have been highlighted and unhighlighted.
  const onNodeHighlight = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  const onNodeUnhighlight = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  onceNewNodeFront = inspector.selection.once("new-node-front");

  clickOpenInspectorIcon(sidebarPanelContent);

  nodeFront = await onceNewNodeFront;
  is(nodeFront.displayName, "body", "The correct node has been selected");
  const { nodeFront: highlightedNodeFront } = await onNodeHighlight;
  is(
    highlightedNodeFront.displayName,
    "body",
    "The correct node was highlighted"
  );

  await onNodeUnhighlight;
});

add_task(async function testSidebarSetExtensionPage() {
  const inspectedWindowFront = await toolbox.target.getFront(
    "webExtensionInspectedWindow"
  );

  const sidebar = inspector.getPanel(SIDEBAR_ID);
  const sidebarPanelContent = inspector.sidebar.getTabPanel(SIDEBAR_ID);

  info("Testing sidebar.setExtensionPage");

  await SpecialPowers.pushPrefEnv({
    set: [["security.allow_unsafe_parent_loads", true]],
  });

  const expectedURL =
    "data:text/html,<!DOCTYPE html><html><body><h1>Extension Page";

  sidebar.setExtensionPage(expectedURL);

  await testSetExtensionPageSidebarPanel(sidebarPanelContent, expectedURL);

  inspectedWindowFront.destroy();
  await SpecialPowers.popPrefEnv();
});

add_task(async function teardownExtensionSidebar() {
  info("Remove the sidebar instance");

  toolbox.unregisterInspectorExtensionSidebar(SIDEBAR_ID);

  ok(
    !inspector.sidebar.getTabPanel(SIDEBAR_ID),
    "The rendered extension sidebar has been removed"
  );

  const inspectorStoreState = inspector.store.getState();

  Assert.deepEqual(
    inspectorStoreState.extensionsSidebar,
    {},
    "The extensions sidebar Redux store data has been cleared"
  );

  await extension.unload();

  toolbox = null;
  inspector = null;
  extension = null;
});

add_task(async function testActiveTabOnNonExistingSidebar() {
  // Set a fake non existing sidebar id in the activeSidebar pref,
  // to simulate the scenario where an extension has installed a sidebar
  // which has been saved in the preference but it doesn't exist anymore.
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.inspector.activeSidebar", "unexisting-sidebar-id"]],
  });

  const res = await openInspectorForURL("about:blank");
  inspector = res.inspector;
  toolbox = res.toolbox;

  const onceSidebarCreated = toolbox.once(
    `extension-sidebar-created-${SIDEBAR_ID}`
  );
  toolbox.registerInspectorExtensionSidebar(SIDEBAR_ID, {
    title: SIDEBAR_TITLE,
  });

  // Wait the extension sidebar to be created and then unregister it to force the tabbar
  // to select a new one.
  await onceSidebarCreated;
  toolbox.unregisterInspectorExtensionSidebar(SIDEBAR_ID);

  is(
    inspector.sidebar.getCurrentTabID(),
    "layoutview",
    "Got the expected inspector sidebar tab selected"
  );

  await SpecialPowers.popPrefEnv();
});
