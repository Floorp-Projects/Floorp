/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* globals getTestActor, openToolboxForTab, gBrowser */
/* import-globals-from ../../shared/test/shared-head.js */

var {
  getInplaceEditorForSpan: inplaceEditor,
} = require("devtools/client/shared/inplace-editor");

// This file contains functions related to the inspector that are also of interest to
// other test directores as well.

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = async function(hostType) {
  info("Opening the inspector");

  const toolbox = await openToolboxForTab(
    gBrowser.selectedTab,
    "inspector",
    hostType
  );
  const inspector = toolbox.getPanel("inspector");

  const testActor = await getTestActor(toolbox);

  return { toolbox, inspector, testActor };
};

/**
 * Open the toolbox, with the inspector tool visible, and the one of the sidebar
 * tabs selected.
 *
 * @param {String} id
 *        The ID of the sidebar tab to be opened
 * @return a promise that resolves when the inspector is ready and the tab is
 * visible and ready
 */
var openInspectorSidebarTab = async function(id) {
  const { toolbox, inspector, testActor } = await openInspector();

  info("Selecting the " + id + " sidebar");

  const onSidebarSelect = inspector.sidebar.once("select");
  if (id === "layoutview") {
    // The layout view should wait until the box-model and grid-panel are ready.
    const onBoxModelViewReady = inspector.once("boxmodel-view-updated");
    const onGridPanelReady = inspector.once("grid-panel-updated");
    inspector.sidebar.select(id);
    await onBoxModelViewReady;
    await onGridPanelReady;
  } else {
    inspector.sidebar.select(id);
  }
  await onSidebarSelect;

  return {
    toolbox,
    inspector,
    testActor,
  };
};

/**
 * Open the toolbox, with the inspector tool visible, and the rule-view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the rule view
 * is visible and ready
 */
function openRuleView() {
  return openInspector().then(data => {
    const view = data.inspector.getPanel("ruleview").view;

    // Replace the view to use a custom debounce function that can be triggered manually
    // through an additional ".flush()" property.
    view.debounce = manualDebounce();

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view,
    };
  });
}

/**
 * Open the toolbox, with the inspector tool visible, and the computed-view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the computed
 * view is visible and ready
 */
function openComputedView() {
  return openInspectorSidebarTab("computedview").then(data => {
    const view = data.inspector.getPanel("computedview").computedView;

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view,
    };
  });
}

/**
 * Open the toolbox, with the inspector tool visible, and the changes view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the changes
 * view is visible and ready
 */
function openChangesView() {
  return openInspectorSidebarTab("changesview").then(data => {
    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view: data.inspector.getPanel("changesview"),
    };
  });
}

/**
 * Open the toolbox, with the inspector tool visible, and the layout view
 * sidebar tab selected to display the box model view with properties.
 *
 * @return {Promise} a promise that resolves when the inspector is ready and the layout
 *         view is visible and ready.
 */
function openLayoutView() {
  return openInspectorSidebarTab("layoutview").then(data => {
    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      boxmodel: data.inspector.getPanel("boxmodel"),
      gridInspector: data.inspector.getPanel("layoutview").gridInspector,
      flexboxInspector: data.inspector.getPanel("layoutview").flexboxInspector,
      layoutView: data.inspector.getPanel("layoutview"),
      testActor: data.testActor,
    };
  });
}

/**
 * Select the rule view sidebar tab on an already opened inspector panel.
 *
 * @param {InspectorPanel} inspector
 *        The opened inspector panel
 * @return {CssRuleView} the rule view
 */
function selectRuleView(inspector) {
  return inspector.getPanel("ruleview").view;
}

/**
 * Select the computed view sidebar tab on an already opened inspector panel.
 *
 * @param {InspectorPanel} inspector
 *        The opened inspector panel
 * @return {CssComputedView} the computed view
 */
function selectComputedView(inspector) {
  inspector.sidebar.select("computedview");
  return inspector.getPanel("computedview").computedView;
}

/**
 * Select the changes view sidebar tab on an already opened inspector panel.
 *
 * @param {InspectorPanel} inspector
 *        The opened inspector panel
 * @return {ChangesView} the changes view
 */
function selectChangesView(inspector) {
  inspector.sidebar.select("changesview");
  return inspector.getPanel("changesview");
}

/**
 * Select the layout view sidebar tab on an already opened inspector panel.
 *
 * @param  {InspectorPanel} inspector
 * @return {BoxModel} the box model
 */
function selectLayoutView(inspector) {
  inspector.sidebar.select("layoutview");
  return inspector.getPanel("boxmodel");
}

/**
 * Get the NodeFront for a node that matches a given css selector, via the
 * protocol.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves to the NodeFront instance
 */
function getNodeFront(selector, { walker }) {
  if (selector._form) {
    return selector;
  }
  return walker.querySelector(walker.rootNode, selector);
}

/**
 * Set the inspector's current selection to the first match of the given css
 * selector
 *
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox.
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not to highlight the
 *        node upon selection.
 * @param {Boolean} isSlotted
 *        Is the selection representing the slotted version the node.
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = async function(
  selector,
  inspector,
  reason = "test",
  isSlotted
) {
  info("Selecting the node for '" + selector + "'");
  const nodeFront = await getNodeFront(selector, inspector);
  const updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, { reason, isSlotted });
  await updated;
};

/**
 * Get the NodeFront for a node that matches a given css selector inside a
 * given iframe.
 *
 * @param {Array} selectors
 *        Arrays of CSS selectors from the root document to the node.
 *        The last CSS selector of the array is for the node in its frame doc.
 *        The before-last CSS selector is for the frame in its parent frame, etc...
 *        Ex: ["frame.first-frame", ..., "frame.last-frame", ".target-node"]
 * @param {InspectorPanel} inspector
 *        See `selectNode`
 * @return {NodeFront} Resolves the corresponding node front.
 */
async function getNodeFrontInFrames(selectors, inspector) {
  let walker = inspector.walker;
  let rootNode = walker.rootNode;

  // Extract the last selector from the provided array of selectors.
  const nodeSelector = selectors.pop();

  // Remaining selectors should all be frame selectors. Renaming for clarity.
  const frameSelectors = selectors;

  info("Loop through all frame selectors");
  for (const frameSelector of frameSelectors) {
    const url = walker.targetFront.url;
    info(`Find the frame element for selector ${frameSelector} in ${url}`);

    const frameFront = await walker.querySelector(rootNode, frameSelector);

    // For a remote frame, connect to the corresponding frame target.
    // Otherwise, reuse the current targetFront.
    let frameTarget = frameFront.targetFront;
    if (frameFront.remoteFrame) {
      info("Connect to remote frame and retrieve the targetFront");
      frameTarget = await frameFront.connectToRemoteFrame();
    }

    walker = (await frameTarget.getFront("inspector")).walker;

    if (frameFront.remoteFrame) {
      // For remote frames or browser elements, use the walker's rootNode.
      rootNode = walker.rootNode;
    } else {
      // For same-process frames, select the document front as the root node.
      // It is a different node from the walker's rootNode.
      info("Retrieve the children of the frame to find the document node");
      const { nodes } = await walker.children(frameFront);
      rootNode = nodes.find(n => n.nodeType === Node.DOCUMENT_NODE);
    }
  }

  return walker.querySelector(rootNode, nodeSelector);
}

/**
 * Helper to select a node in the markup-view, in a nested tree of
 * frames/browser elements. The iframes can either be remote or same-process.
 *
 * Note: "frame" will refer to either "frame" or "browser" in the documentation
 * and method.
 *
 * @param {Array} selectors
 *        Arrays of CSS selectors from the root document to the node.
 *        The last CSS selector of the array is for the node in its frame doc.
 *        The before-last CSS selector is for the frame in its parent frame, etc...
 *        Ex: ["frame.first-frame", ..., "frame.last-frame", ".target-node"]
 * @param {InspectorPanel} inspector
 *        See `selectNode`
 * @param {String} reason
 *        See `selectNode`
 * @param {Boolean} isSlotted
 *        See `selectNode`
 * @return {NodeFront} The selected node front.
 */
async function selectNodeInFrames(
  selectors,
  inspector,
  reason = "test",
  isSlotted
) {
  const nodeFront = await getNodeFrontInFrames(selectors, inspector);
  await selectNode(nodeFront, inspector, reason, isSlotted);
  return nodeFront;
}

/**
 * Create a throttling function that can be manually "flushed". This is to replace the
 * use of the `debounce` function from `devtools/client/inspector/shared/utils.js`, which
 * has a setTimeout that can cause intermittents.
 * @return {Function} This function has the same function signature as debounce, but
 *                    the property `.flush()` has been added for flushing out any
 *                    debounced calls.
 */
function manualDebounce() {
  let calls = [];

  function debounce(func, wait, scope) {
    return function() {
      const existingCall = calls.find(call => call.func === func);
      if (existingCall) {
        existingCall.args = arguments;
      } else {
        calls.push({ func, wait, scope, args: arguments });
      }
    };
  }

  debounce.flush = function() {
    calls.forEach(({ func, scope, args }) => func.apply(scope, args));
    calls = [];
  };

  return debounce;
}

/**
 * Get the requested rule style property from the current browser.
 *
 * @param {Number} styleSheetIndex
 * @param {Number} ruleIndex
 * @param {String} name
 * @return {String} The value, if found, null otherwise
 */

async function getRulePropertyValue(styleSheetIndex, ruleIndex, name) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [styleSheetIndex, ruleIndex, name],
    (styleSheetIndexChild, ruleIndexChild, nameChild) => {
      let value = null;

      info(
        "Getting the value for property name " +
          nameChild +
          " in sheet " +
          styleSheetIndexChild +
          " and rule " +
          ruleIndexChild
      );

      const sheet = content.document.styleSheets[styleSheetIndexChild];
      if (sheet) {
        const rule = sheet.cssRules[ruleIndexChild];
        if (rule) {
          value = rule.style.getPropertyValue(nameChild);
        }
      }

      return value;
    }
  );
}

/**
 * Get the requested computed style property from the current browser.
 *
 * @param {String} selector
 *        The selector used to obtain the element.
 * @param {String} pseudo
 *        pseudo id to query, or null.
 * @param {String} propName
 *        name of the property.
 */
async function getComputedStyleProperty(selector, pseudo, propName) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, pseudo, propName],
    (selectorChild, pseudoChild, propNameChild) => {
      const element = content.document.querySelector(selectorChild);
      return content.document.defaultView
        .getComputedStyle(element, pseudoChild)
        .getPropertyValue(propNameChild);
    }
  );
}

/**
 * Wait until the requested computed style property has the
 * expected value in the the current browser.
 *
 * @param {String} selector
 *        The selector used to obtain the element.
 * @param {String} pseudo
 *        pseudo id to query, or null.
 * @param {String} propName
 *        name of the property.
 * @param {String} expected
 *        expected value of property
 */
async function waitForComputedStyleProperty(
  selector,
  pseudo,
  propName,
  expected
) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, pseudo, propName, expected],
    (selectorChild, pseudoChild, propNameChild, expectedChild) => {
      const element = content.document.querySelector(selectorChild);
      return ContentTaskUtils.waitForCondition(() => {
        const value = content.document.defaultView
          .getComputedStyle(element, pseudoChild)
          .getPropertyValue(propNameChild);
        return value === expectedChild;
      });
    }
  );
}

/**
 * Given an inplace editable element, click to switch it to edit mode, wait for
 * focus
 *
 * @return a promise that resolves to the inplace-editor element when ready
 */
var focusEditableField = async function(
  ruleView,
  editable,
  xOffset = 1,
  yOffset = 1,
  options = {}
) {
  const onFocus = once(editable.parentNode, "focus", true);
  info("Clicking on editable field to turn to edit mode");
  if (options.type === undefined) {
    // "mousedown" and "mouseup" flushes any pending layout.  Therefore,
    // if the caller wants to click an element, e.g., closebrace to add new
    // property, we need to guarantee that the element is clicked here even
    // if it's moved by flushing the layout because whether the UI is useful
    // or not when there is pending reflow is not scope of the tests.
    options.type = "mousedown";
    EventUtils.synthesizeMouse(
      editable,
      xOffset,
      yOffset,
      options,
      editable.ownerGlobal
    );
    options.type = "mouseup";
    EventUtils.synthesizeMouse(
      editable,
      xOffset,
      yOffset,
      options,
      editable.ownerGlobal
    );
  } else {
    EventUtils.synthesizeMouse(
      editable,
      xOffset,
      yOffset,
      options,
      editable.ownerGlobal
    );
  }
  await onFocus;

  info("Editable field gained focus, returning the input field now");
  const onEdit = inplaceEditor(editable.ownerDocument.activeElement);

  return onEdit;
};

/**
 * Get the DOMNode for a css rule in the rule-view that corresponds to the given
 * selector.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} selectorText
 *        The selector in the rule-view for which the rule
 *        object is wanted
 * @param {Number} index
 *        If there are more than 1 rule with the same selector, you may pass a
 *        index to determine which of the rules you want.
 * @return {DOMNode}
 */
function getRuleViewRule(view, selectorText, index = 0) {
  let rule;
  let pos = 0;
  for (const r of view.styleDocument.querySelectorAll(".ruleview-rule")) {
    const selector = r.querySelector(
      ".ruleview-selectorcontainer, " + ".ruleview-selector-matched"
    );
    if (selector && selector.textContent === selectorText) {
      if (index == pos) {
        rule = r;
        break;
      }
      pos++;
    }
  }

  return rule;
}

/**
 * Get references to the name and value span nodes corresponding to a given
 * selector and property name in the rule-view
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} selectorText
 *        The selector in the rule-view to look for the property in
 * @param {String} propertyName
 *        The name of the property
 * @return {Object} An object like {nameSpan: DOMNode, valueSpan: DOMNode}
 */
function getRuleViewProperty(view, selectorText, propertyName) {
  let prop;

  const rule = getRuleViewRule(view, selectorText);
  if (rule) {
    // Look for the propertyName in that rule element
    for (const p of rule.querySelectorAll(".ruleview-property")) {
      const nameSpan = p.querySelector(".ruleview-propertyname");
      const valueSpan = p.querySelector(".ruleview-propertyvalue");

      if (nameSpan.textContent === propertyName) {
        prop = { nameSpan: nameSpan, valueSpan: valueSpan };
        break;
      }
    }
  }
  return prop;
}

/**
 * Get the text value of the property corresponding to a given selector and name
 * in the rule-view
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} selectorText
 *        The selector in the rule-view to look for the property in
 * @param {String} propertyName
 *        The name of the property
 * @return {String} The property value
 */
function getRuleViewPropertyValue(view, selectorText, propertyName) {
  return getRuleViewProperty(view, selectorText, propertyName).valueSpan
    .textContent;
}

/**
 * Get a reference to the selector DOM element corresponding to a given selector
 * in the rule-view
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} selectorText
 *        The selector in the rule-view to look for
 * @return {DOMNode} The selector DOM element
 */
function getRuleViewSelector(view, selectorText) {
  const rule = getRuleViewRule(view, selectorText);
  return rule.querySelector(".ruleview-selector, .ruleview-selector-matched");
}

/**
 * Get a rule-link from the rule-view given its index
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {Number} index
 *        The index of the link to get
 * @return {DOMNode} The link if any at this index
 */
function getRuleViewLinkByIndex(view, index) {
  const links = view.styleDocument.querySelectorAll(".ruleview-rule-source");
  return links[index];
}

/**
 * Get rule-link text from the rule-view given its index
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {Number} index
 *        The index of the link to get
 * @return {String} The string at this index
 */
function getRuleViewLinkTextByIndex(view, index) {
  const link = getRuleViewLinkByIndex(view, index);
  return link.querySelector(".ruleview-rule-source-label").textContent;
}

/**
 * Click on a rule-view's close brace to focus a new property name editor
 *
 * @param {RuleEditor} ruleEditor
 *        An instance of RuleEditor that will receive the new property
 * @return a promise that resolves to the newly created editor when ready and
 * focused
 */
var focusNewRuleViewProperty = async function(ruleEditor) {
  info("Clicking on a close ruleEditor brace to start editing a new property");

  // Use bottom alignment to avoid scrolling out of the parent element area.
  ruleEditor.closeBrace.scrollIntoView(false);
  const editor = await focusEditableField(
    ruleEditor.ruleView,
    ruleEditor.closeBrace
  );

  is(
    inplaceEditor(ruleEditor.newPropSpan),
    editor,
    "Focused editor is the new property editor."
  );

  return editor;
};

/**
 * Create a new property name in the rule-view, focusing a new property editor
 * by clicking on the close brace, and then entering the given text.
 * Keep in mind that the rule-view knows how to handle strings with multiple
 * properties, so the input text may be like: "p1:v1;p2:v2;p3:v3".
 *
 * @param {RuleEditor} ruleEditor
 *        The instance of RuleEditor that will receive the new property(ies)
 * @param {String} inputValue
 *        The text to be entered in the new property name field
 * @return a promise that resolves when the new property name has been entered
 * and once the value field is focused
 */
var createNewRuleViewProperty = async function(ruleEditor, inputValue) {
  info("Creating a new property editor");
  const editor = await focusNewRuleViewProperty(ruleEditor);

  info("Entering the value " + inputValue);
  editor.input.value = inputValue;

  info("Submitting the new value and waiting for value field focus");
  const onFocus = once(ruleEditor.element, "focus", true);
  EventUtils.synthesizeKey(
    "VK_RETURN",
    {},
    ruleEditor.element.ownerDocument.defaultView
  );
  await onFocus;
};

/**
 * Set the search value for the rule-view filter styles search box.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} searchValue
 *        The filter search value
 * @return a promise that resolves when the rule-view is filtered for the
 * search term
 */
var setSearchFilter = async function(view, searchValue) {
  info('Setting filter text to "' + searchValue + '"');

  const searchField = view.searchField;
  searchField.focus();

  for (const key of searchValue.split("")) {
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
  }

  await view.inspector.once("ruleview-filtered");
};

/**
 * Flatten all context menu items into a single array to make searching through
 * it easier.
 */
function buildContextMenuItems(menu) {
  const allItems = [].concat.apply(
    [],
    menu.items.map(function addItem(item) {
      if (item.submenu) {
        return addItem(item.submenu.items);
      }
      return item;
    })
  );

  return allItems;
}

/**
 * Open the style editor context menu and return all of it's items in a flat array
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @return An array of MenuItems
 */
function openStyleContextMenuAndGetAllItems(view, target) {
  const menu = view.contextMenu._openMenu({ target: target });
  return buildContextMenuItems(menu);
}

/**
 * Open the inspector menu and return all of it's items in a flat array
 * @param {InspectorPanel} inspector
 * @param {Object} options to pass into openMenu
 * @return An array of MenuItems
 */
function openContextMenuAndGetAllItems(inspector, options) {
  const menu = inspector.markup.contextMenu._openMenu(options);
  return buildContextMenuItems(menu);
}

/**
 * Wait until the elements the given selectors indicate come to have the visited state.
 *
 * @param {Tab} tab
 *        The tab where the elements on.
 * @param {Array} selectors
 *        The selectors for the elements.
 */
async function waitUntilVisitedState(tab, selectors) {
  await asyncWaitUntil(async () => {
    const hasVisitedState = await ContentTask.spawn(
      tab.linkedBrowser,
      selectors,
      args => {
        const NS_EVENT_STATE_VISITED = 1 << 24;

        for (const selector of args) {
          const target = content.wrappedJSObject.document.querySelector(
            selector
          );
          if (
            !(
              target &&
              InspectorUtils.getContentState(target) & NS_EVENT_STATE_VISITED
            )
          ) {
            return false;
          }
        }
        return true;
      }
    );
    return hasVisitedState;
  });
}
