/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

var {CssRuleView} = require("devtools/client/inspector/rules/rules");
var {getInplaceEditorForSpan: inplaceEditor} =
  require("devtools/client/shared/inplace-editor");
const {getColor: getThemeColor} = require("devtools/client/shared/theme");

const TEST_URL_ROOT =
  "http://example.com/browser/devtools/client/inspector/shared/test/";
const TEST_URL_ROOT_SSL =
  "https://example.com/browser/devtools/client/inspector/shared/test/";
const ROOT_TEST_DIR = getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL = ROOT_TEST_DIR + "doc_frame_script.js";
const STYLE_INSPECTOR_L10N =
      new LocalizationHelper("devtools-shared/locale/styleinspector.properties");

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
});

/**
 * The functions found below are here to ease test development and maintenance.
 * Most of these functions are stateless and will require some form of context
 * (the instance of the current toolbox, or inspector panel for instance).
 *
 * Most of these functions are async too and return promises.
 *
 * All tests should follow the following pattern:
 *
 * add_task(function*() {
 *   yield addTab(TEST_URI);
 *   let {toolbox, inspector} = yield openInspector();
 *   inspector.sidebar.select(viewId);
 *   let view = inspector[viewId].view;
 *   yield selectNode("#test", inspector);
 *   yield someAsyncTestFunction(view);
 * });
 *
 * add_task is the way to define the testcase in the test file. It accepts
 * a single generator-function argument.
 * The generator function should yield any async call.
 *
 * There is no need to clean tabs up at the end of a test as this is done
 * automatically.
 *
 * It is advised not to store any references on the global scope. There
 * shouldn't be a need to anyway. Thanks to add_task, test steps, even
 * though asynchronous, can be described in a nice flat way, and
 * if/for/while/... control flow can be used as in sync code, making it
 * possible to write the outline of the test case all in add_task, and delegate
 * actual processing and assertions to other functions.
 */

/* *********************************************
 * UTILS
 * *********************************************
 * General test utilities.
 * Add new tabs, open the toolbox and switch to the various panels, select
 * nodes, get node references, ...
 */

/**
 * The rule-view tests rely on a frame-script to be injected in the content test
 * page. So override the shared-head's addTab to load the frame script after the
 * tab was added.
 * FIXME: Refactor the rule-view tests to use the testActor instead of a frame
 * script, so they can run on remote targets too.
 */
var _addTab = addTab;
addTab = function (url) {
  return _addTab(url).then(tab => {
    info("Loading the helper frame script " + FRAME_SCRIPT_URL);
    let browser = tab.linkedBrowser;
    browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
    return tab;
  });
};

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
 * Polls a given function waiting for it to return true.
 *
 * @param {Function} validatorFn
 *        A validator function that returns a boolean.
 *        This is called every few milliseconds to check if the result is true.
 *        When it is true, the promise resolves.
 * @param {String} name
 *        Optional name of the test. This is used to generate
 *        the success and failure messages.
 * @return a promise that resolves when the function returned true or rejects
 * if the timeout is reached
 */
function waitForSuccess(validatorFn, name = "untitled") {
  let def = defer();

  function wait(validator) {
    if (validator()) {
      ok(true, "Validator function " + name + " returned true");
      def.resolve();
    } else {
      setTimeout(() => wait(validator), 200);
    }
  }
  wait(validatorFn);

  return def.promise;
}

/**
 * Get the dataURL for the font family tooltip.
 *
 * @param {String} font
 *        The font family value.
 * @param {object} nodeFront
 *        The NodeActor that will used to retrieve the dataURL for the
 *        font family tooltip contents.
 */
var getFontFamilyDataURL = Task.async(function* (font, nodeFront) {
  let fillStyle = getThemeColor("body-color");

  let {data} = yield nodeFront.getFontFamilyDataURL(font, fillStyle);
  let dataURL = yield data.string();
  return dataURL;
});

/* *********************************************
 * RULE-VIEW
 * *********************************************
 * Rule-view related test utility functions
 * This object contains functions to get rules, get properties, ...
 */

/**
 * Get the DOMNode for a css rule in the rule-view that corresponds to the given
 * selector
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} selectorText
 *        The selector in the rule-view for which the rule
 *        object is wanted
 * @return {DOMNode}
 */
function getRuleViewRule(view, selectorText) {
  let rule;
  for (let r of view.styleDocument.querySelectorAll(".ruleview-rule")) {
    let selector = r.querySelector(".ruleview-selectorcontainer, " +
                                   ".ruleview-selector-matched");
    if (selector && selector.textContent === selectorText) {
      rule = r;
      break;
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
 * @return {DOMNode} The selectorhighlighter icon DOM element
 */
function getRuleViewSelectorHighlighterIcon(view, selectorText) {
  let rule = getRuleViewRule(view, selectorText);
  return rule.querySelector(".ruleview-selectorhighlighter");
}

/**
 * Simulate a color change in a given color picker tooltip, and optionally wait
 * for a given element in the page to have its style changed as a result
 *
 * @param {RuleView} ruleView
 *        The related rule view instance
 * @param {SwatchColorPickerTooltip} colorPicker
 * @param {Array} newRgba
 *        The new color to be set [r, g, b, a]
 * @param {Object} expectedChange
 *        Optional object that needs the following props:
 *          - {DOMNode} element The element in the page that will have its
 *            style changed.
 *          - {String} name The style name that will be changed
 *          - {String} value The expected style value
 * The style will be checked like so: getComputedStyle(element)[name] === value
 */
var simulateColorPickerChange = Task.async(function* (ruleView, colorPicker,
    newRgba, expectedChange) {
  let onRuleViewChanged = ruleView.once("ruleview-changed");
  info("Getting the spectrum colorpicker object");
  let spectrum = yield colorPicker.spectrum;
  info("Setting the new color");
  spectrum.rgb = newRgba;
  info("Applying the change");
  spectrum.updateUI();
  spectrum.onChange();
  info("Waiting for rule-view to update");
  yield onRuleViewChanged;

  if (expectedChange) {
    info("Waiting for the style to be applied on the page");
    yield waitForSuccess(() => {
      let {element, name, value} = expectedChange;
      return content.getComputedStyle(element)[name] === value;
    }, "Color picker change applied on the page");
  }
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
  ruleEditor.closeBrace.scrollIntoView();
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
  let win = view.styleWindow;
  let searchField = view.searchField;
  searchField.focus();
  synthesizeKeys(searchValue, win);
  yield view.inspector.once("ruleview-filtered");
});

/* *********************************************
 * COMPUTED-VIEW
 * *********************************************
 * Computed-view related utility functions.
 * Allows to get properties, links, expand properties, ...
 */

/**
 * Get references to the name and value span nodes corresponding to a given
 * property name in the computed-view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return an object {nameSpan, valueSpan}
 */
function getComputedViewProperty(view, name) {
  let prop;
  for (let property of view.styleDocument.querySelectorAll(".property-view")) {
    let nameSpan = property.querySelector(".property-name");
    let valueSpan = property.querySelector(".property-value");

    if (nameSpan.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
      break;
    }
  }
  return prop;
}

/**
 * Get the text value of the property corresponding to a given name in the
 * computed-view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {String} The property value
 */
function getComputedViewPropertyValue(view, name, propertyName) {
  return getComputedViewProperty(view, name, propertyName)
    .valueSpan.textContent;
}

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
