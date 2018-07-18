/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* globals registerTestActor, getTestActor, openToolboxForTab, gBrowser */
/* import-globals-from ../../shared/test/shared-head.js */
/* import-globals-from ../../shared/test/test-actor-registry.js */

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

var {getInplaceEditorForSpan: inplaceEditor} = require("devtools/client/shared/inplace-editor");

// This file contains functions related to the inspector that are also of interest to
// other test directores as well.

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = async function(hostType) {
  info("Opening the inspector");

  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "inspector",
                                        hostType);
  const inspector = toolbox.getPanel("inspector");

  if (inspector._updateProgress) {
    info("Need to wait for the inspector to update");
    await inspector.once("inspector-updated");
  }

  await registerTestActor(toolbox.target.client);
  const testActor = await getTestActor(toolbox);

  return {toolbox, inspector, testActor};
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
  const {toolbox, inspector, testActor} = await openInspector();

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
    testActor
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

    // Adds the highlighters overlay in the rule view.
    view.addHighlightersToView();

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
    // Adds the highlighters overlay in the computed view.
    view.addHighlightersToView();

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view,
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
    // The actual highligher show/hide methods are mocked in box model tests.
    // The highlighter is tested in devtools/inspector/test.
    function mockHighlighter({highlighter}) {
      highlighter.showBoxModel = function() {
        return promise.resolve();
      };
      highlighter.hideBoxModel = function() {
        return promise.resolve();
      };
    }
    mockHighlighter(data.toolbox);

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      boxmodel: data.inspector.getPanel("boxmodel"),
      gridInspector: data.inspector.layoutview.gridInspector,
      layoutView: data.inspector.layoutview,
      testActor: data.testActor
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
  const view = inspector.getPanel("ruleview").view;
  view.addHighlightersToView();
  return view;
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
  const view = inspector.getPanel("computedview").computedView;
  view.addHighlightersToView();
  return view;
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
function getNodeFront(selector, {walker}) {
  if (selector._form) {
    return selector;
  }
  return walker.querySelector(walker.rootNode, selector);
}

/**
 * Set the inspector's current selection to the first match of the given css
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = async function(selector, inspector, reason = "test", isSlotted) {
  info("Selecting the node for '" + selector + "'");
  const nodeFront = await getNodeFront(selector, inspector);
  const updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, { reason, isSlotted });
  await updated;
};

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
    calls.forEach(({func, scope, args}) => func.apply(scope, args));
    calls = [];
  };

  return debounce;
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 *
 * @param {String} name
 *        The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  info("Expecting message " + name + " from content");

  const mm = gBrowser.selectedBrowser.messageManager;

  return new Promise(resolve => {
    mm.addMessageListener(name, function onMessage(msg) {
      mm.removeMessageListener(name, onMessage);
      resolve(msg.data);
    });
  });
}

/**
 * Send an async message to the frame script (chrome -> content) and wait for a
 * response message with the same name (content -> chrome).
 *
 * @param {String} name
 *        The message name. Should be one of the messages defined
 *        in doc_frame_script.js
 * @param {Object} data
 *        Optional data to send along
 * @param {Object} objects
 *        Optional CPOW objects to send along
 * @param {Boolean} expectResponse
 *        If set to false, don't wait for a response with the same name
 *        from the content script. Defaults to true.
 * @return {Promise} Resolves to the response data if a response is expected,
 * immediately resolves otherwise
 */
function executeInContent(name, data = {}, objects = {},
                          expectResponse = true) {
  info("Sending message " + name + " to content");
  const mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  }

  return promise.resolve();
}

/**
 * Send an async message to the frame script and get back the requested
 * computed style property.
 *
 * @param {String} selector
 *        The selector used to obtain the element.
 * @param {String} pseudo
 *        pseudo id to query, or null.
 * @param {String} name
 *        name of the property.
 */
async function getComputedStyleProperty(selector, pseudo, propName) {
  return executeInContent("Test:GetComputedStylePropertyValue",
    {selector,
     pseudo,
     name: propName});
}

/**
 * Send an async message to the frame script and wait until the requested
 * computed style property has the expected value.
 *
 * @param {String} selector
 *        The selector used to obtain the element.
 * @param {String} pseudo
 *        pseudo id to query, or null.
 * @param {String} prop
 *        name of the property.
 * @param {String} expected
 *        expected value of property
 * @param {String} name
 *        the name used in test message
 */
async function waitForComputedStyleProperty(selector, pseudo, name, expected) {
  return executeInContent("Test:WaitForComputedStylePropertyValue",
    {selector,
     pseudo,
     expected,
     name});
}

/**
 * Given an inplace editable element, click to switch it to edit mode, wait for
 * focus
 *
 * @return a promise that resolves to the inplace-editor element when ready
 */
var focusEditableField = async function(ruleView, editable, xOffset = 1,
    yOffset = 1, options = {}) {
  const onFocus = once(editable.parentNode, "focus", true);
  info("Clicking on editable field to turn to edit mode");
  EventUtils.synthesizeMouse(editable, xOffset, yOffset, options,
    editable.ownerDocument.defaultView);
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
    const selector = r.querySelector(".ruleview-selectorcontainer, " +
                                   ".ruleview-selector-matched");
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
        prop = {nameSpan: nameSpan, valueSpan: valueSpan};
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
  return getRuleViewProperty(view, selectorText, propertyName)
    .valueSpan.textContent;
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
 * Get a reference to the selectorhighlighter icon DOM element corresponding to
 * a given selector in the rule-view
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} selectorText
 *        The selector in the rule-view to look for
 * @param {Number} index
 *        If there are more than 1 rule with the same selector, use this index
 *        to determine which one should be retrieved. Defaults to 0
 * @return {DOMNode} The selectorhighlighter icon DOM element
 */
var getRuleViewSelectorHighlighterIcon = async function(view,
    selectorText, index = 0) {
  const rule = getRuleViewRule(view, selectorText, index);

  const editor = rule._ruleEditor;
  if (!editor.uniqueSelector) {
    await once(editor, "selector-icon-created");
  }

  return rule.querySelector(".ruleview-selectorhighlighter");
};

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
  const editor = await focusEditableField(ruleEditor.ruleView,
    ruleEditor.closeBrace);

  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "Focused editor is the new property editor.");

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
  EventUtils.synthesizeKey("VK_RETURN", {},
    ruleEditor.element.ownerDocument.defaultView);
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
  info("Setting filter text to \"" + searchValue + "\"");

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
  const allItems = [].concat.apply([], menu.items.map(function addItem(item) {
    if (item.submenu) {
      return addItem(item.submenu.items);
    }
    return item;
  }));

  return allItems;
}

/**
 * Open the style editor context menu and return all of it's items in a flat array
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @return An array of MenuItems
 */
function openStyleContextMenuAndGetAllItems(view, target) {
  const menu = view.contextMenu._openMenu({target: target});
  return buildContextMenuItems(menu);
}

/**
 * Open the inspector menu and return all of it's items in a flat array
 * @param {InspectorPanel} inspector
 * @param {Object} options to pass into openMenu
 * @return An array of MenuItems
 */
function openContextMenuAndGetAllItems(inspector, options) {
  const menu = inspector._openMenu(options);
  return buildContextMenuItems(menu);
}
