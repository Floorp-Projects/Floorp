/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* globals registerTestActor, getTestActor, Task, openToolboxForTab, gBrowser */
/* import-globals-from ../../framework/test/shared-head.js */

var {getInplaceEditorForSpan: inplaceEditor} = require("devtools/client/shared/inplace-editor");

// This file contains functions related to the inspector that are also of interest to
// other test directores as well.

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = Task.async(function* (hostType) {
  info("Opening the inspector");

  let toolbox = yield openToolboxForTab(gBrowser.selectedTab, "inspector",
                                        hostType);
  let inspector = toolbox.getPanel("inspector");

  if (inspector._updateProgress) {
    info("Need to wait for the inspector to update");
    yield inspector.once("inspector-updated");
  }

  info("Waiting for actor features to be detected");
  yield inspector._detectingActorFeatures;

  yield registerTestActor(toolbox.target.client);
  let testActor = yield getTestActor(toolbox);

  return {toolbox, inspector, testActor};
});

/**
 * Open the toolbox, with the inspector tool visible, and the one of the sidebar
 * tabs selected.
 *
 * @param {String} id
 *        The ID of the sidebar tab to be opened
 * @return a promise that resolves when the inspector is ready and the tab is
 * visible and ready
 */
var openInspectorSidebarTab = Task.async(function* (id) {
  let {toolbox, inspector, testActor} = yield openInspector();

  info("Selecting the " + id + " sidebar");

  if (id === "computedview" || id === "layoutview") {
    // The layout and computed views should wait until the box-model widget is ready.
    let onBoxModelViewReady = inspector.once("boxmodel-view-updated");
    inspector.sidebar.select(id);
    yield onBoxModelViewReady;
  } else {
    inspector.sidebar.select(id);
  }

  return {
    toolbox,
    inspector,
    testActor
  };
});

/**
 * Open the toolbox, with the inspector tool visible, and the rule-view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the rule view
 * is visible and ready
 */
function openRuleView() {
  return openInspectorSidebarTab("ruleview").then(data => {
    // Replace the view to use a custom debounce function that can be triggered manually
    // through an additional ".flush()" property.
    data.inspector.getPanel("ruleview").view.debounce = manualDebounce();

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view: data.inspector.getPanel("ruleview").view
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
    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view: data.inspector.getPanel("computedview").computedView
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
      highlighter.showBoxModel = function () {
        return promise.resolve();
      };
      highlighter.hideBoxModel = function () {
        return promise.resolve();
      };
    }
    mockHighlighter(data.toolbox);

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      boxmodel: data.inspector.getPanel("boxmodel"),
      gridInspector: data.inspector.gridInspector,
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
  inspector.sidebar.select("ruleview");
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
var selectNode = Task.async(function* (selector, inspector, reason = "test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

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
    return function () {
      let existingCall = calls.find(call => call.func === func);
      if (existingCall) {
        existingCall.args = arguments;
      } else {
        calls.push({ func, wait, scope, args: arguments });
      }
    };
  }

  debounce.flush = function () {
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

  let mm = gBrowser.selectedBrowser.messageManager;

  let def = defer();
  mm.addMessageListener(name, function onMessage(msg) {
    mm.removeMessageListener(name, onMessage);
    def.resolve(msg.data);
  });
  return def.promise;
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
  let mm = gBrowser.selectedBrowser.messageManager;

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
function* getComputedStyleProperty(selector, pseudo, propName) {
  return yield executeInContent("Test:GetComputedStylePropertyValue",
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
function* waitForComputedStyleProperty(selector, pseudo, name, expected) {
  return yield executeInContent("Test:WaitForComputedStylePropertyValue",
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
var focusEditableField = Task.async(function* (ruleView, editable, xOffset = 1,
    yOffset = 1, options = {}) {
  let onFocus = once(editable.parentNode, "focus", true);
  info("Clicking on editable field to turn to edit mode");
  EventUtils.synthesizeMouse(editable, xOffset, yOffset, options,
    editable.ownerDocument.defaultView);
  yield onFocus;

  info("Editable field gained focus, returning the input field now");
  let onEdit = inplaceEditor(editable.ownerDocument.activeElement);

  return onEdit;
});

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
  for (let r of view.styleDocument.querySelectorAll(".ruleview-rule")) {
    let selector = r.querySelector(".ruleview-selectorcontainer, " +
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

  let rule = getRuleViewRule(view, selectorText);
  if (rule) {
    // Look for the propertyName in that rule element
    for (let p of rule.querySelectorAll(".ruleview-property")) {
      let nameSpan = p.querySelector(".ruleview-propertyname");
      let valueSpan = p.querySelector(".ruleview-propertyvalue");

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
  let rule = getRuleViewRule(view, selectorText);
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
var getRuleViewSelectorHighlighterIcon = Task.async(function* (view,
    selectorText, index = 0) {
  let rule = getRuleViewRule(view, selectorText, index);

  let editor = rule._ruleEditor;
  if (!editor.uniqueSelector) {
    yield once(editor, "selector-icon-created");
  }

  return rule.querySelector(".ruleview-selectorhighlighter");
});

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
  let links = view.styleDocument.querySelectorAll(".ruleview-rule-source");
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
  let link = getRuleViewLinkByIndex(view, index);
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
var focusNewRuleViewProperty = Task.async(function* (ruleEditor) {
  info("Clicking on a close ruleEditor brace to start editing a new property");

  // Use bottom alignment to avoid scrolling out of the parent element area.
  ruleEditor.closeBrace.scrollIntoView(false);
  let editor = yield focusEditableField(ruleEditor.ruleView,
    ruleEditor.closeBrace);

  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "Focused editor is the new property editor.");

  return editor;
});

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
var createNewRuleViewProperty = Task.async(function* (ruleEditor, inputValue) {
  info("Creating a new property editor");
  let editor = yield focusNewRuleViewProperty(ruleEditor);

  info("Entering the value " + inputValue);
  editor.input.value = inputValue;

  info("Submitting the new value and waiting for value field focus");
  let onFocus = once(ruleEditor.element, "focus", true);
  EventUtils.synthesizeKey("VK_RETURN", {},
    ruleEditor.element.ownerDocument.defaultView);
  yield onFocus;
});

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
var setSearchFilter = Task.async(function* (view, searchValue) {
  info("Setting filter text to \"" + searchValue + "\"");

  let searchField = view.searchField;
  searchField.focus();

  for (let key of searchValue.split("")) {
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
  }

  yield view.inspector.once("ruleview-filtered");
});

/**
 * Open the style editor context menu and return all of it's items in a flat array
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @return An array of MenuItems
 */
function openStyleContextMenuAndGetAllItems(view, target) {
  let menu = view._contextmenu._openMenu({target: target});

  // Flatten all menu items into a single array to make searching through it easier
  let allItems = [].concat.apply([], menu.items.map(function addItem(item) {
    if (item.submenu) {
      return addItem(item.submenu.items);
    }
    return item;
  }));

  return allItems;
}
