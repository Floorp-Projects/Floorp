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

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
});

var {getInplaceEditorForSpan: inplaceEditor} =
  require("devtools/client/shared/inplace-editor");

const ROOT_TEST_DIR = getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL = ROOT_TEST_DIR + "doc_frame_script.js";

const STYLE_INSPECTOR_L10N
      = new LocalizationHelper("devtools/shared/locales/styleinspector.properties");

Services.prefs.setBoolPref("devtools.inspector.shapesHighlighter.enabled", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
  Services.prefs.clearUserPref("devtools.inspector.shapesHighlighter.enabled");
});

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
 * Get an element's inline style property value.
 * @param {TestActor} testActor
 * @param {String} selector
 *        The selector used to obtain the element.
 * @param {String} name
 *        name of the property.
 */
function getStyle(testActor, selector, propName) {
  return testActor.eval(`
    content.document.querySelector("${selector}")
                    .style.getPropertyValue("${propName}");
  `);
}

/**
 * When a tooltip is closed, this ends up "commiting" the value changed within
 * the tooltip (e.g. the color in case of a colorpicker) which, in turn, ends up
 * setting the value of the corresponding css property in the rule-view.
 * Use this function to close the tooltip and make sure the test waits for the
 * ruleview-changed event.
 * @param {SwatchBasedEditorTooltip} editorTooltip
 * @param {CSSRuleView} view
 */
function* hideTooltipAndWaitForRuleViewChanged(editorTooltip, view) {
  let onModified = view.once("ruleview-changed");
  let onHidden = editorTooltip.tooltip.once("hidden");
  editorTooltip.hide();
  yield onModified;
  yield onHidden;
}

/**
 * Polls a given generator function waiting for it to return true.
 *
 * @param {Function} validatorFn
 *        A validator generator function that returns a boolean.
 *        This is called every few milliseconds to check if the result is true.
 *        When it is true, the promise resolves.
 * @param {String} name
 *        Optional name of the test. This is used to generate
 *        the success and failure messages.
 * @return a promise that resolves when the function returned true or rejects
 * if the timeout is reached
 */
var waitForSuccess = Task.async(function* (validatorFn, desc = "untitled") {
  let i = 0;
  while (true) {
    info("Checking: " + desc);
    if (yield validatorFn()) {
      ok(true, "Success: " + desc);
      break;
    }
    i++;
    if (i > 10) {
      ok(false, "Failure: " + desc);
      break;
    }
    yield new Promise(r => setTimeout(r, 200));
  }
});

/**
 * Simulate a color change in a given color picker tooltip, and optionally wait
 * for a given element in the page to have its style changed as a result.
 * Note that this function assumes that the colorpicker popup is already open
 * and it won't close it after having selected the new color.
 *
 * @param {RuleView} ruleView
 *        The related rule view instance
 * @param {SwatchColorPickerTooltip} colorPicker
 * @param {Array} newRgba
 *        The new color to be set [r, g, b, a]
 * @param {Object} expectedChange
 *        Optional object that needs the following props:
 *          - {String} selector The selector to the element in the page that
 *            will have its style changed.
 *          - {String} name The style name that will be changed
 *          - {String} value The expected style value
 * The style will be checked like so: getComputedStyle(element)[name] === value
 */
var simulateColorPickerChange = Task.async(function* (ruleView, colorPicker,
    newRgba, expectedChange) {
  let onComputedStyleChanged;
  if (expectedChange) {
    let {selector, name, value} = expectedChange;
    onComputedStyleChanged = waitForComputedStyleProperty(selector, null, name, value);
  }
  let onRuleViewChanged = ruleView.once("ruleview-changed");
  info("Getting the spectrum colorpicker object");
  let spectrum = colorPicker.spectrum;
  info("Setting the new color");
  spectrum.rgb = newRgba;
  info("Applying the change");
  spectrum.updateUI();
  spectrum.onChange();
  info("Waiting for rule-view to update");
  yield onRuleViewChanged;

  if (expectedChange) {
    info("Waiting for the style to be applied on the page");
    yield onComputedStyleChanged;
  }
});

/**
 * Open the color picker popup for a given property in a given rule and
 * simulate a color change. Optionally wait for a given element in the page to
 * have its style changed as a result.
 *
 * @param {RuleView} view
 *        The related rule view instance
 * @param {Number} ruleIndex
 *        Which rule to target in the rule view
 * @param {Number} propIndex
 *        Which property to target in the rule
 * @param {Array} newRgba
 *        The new color to be set [r, g, b, a]
 * @param {Object} expectedChange
 *        Optional object that needs the following props:
 *          - {String} selector The selector to the element in the page that
 *            will have its style changed.
 *          - {String} name The style name that will be changed
 *          - {String} value The expected style value
 * The style will be checked like so: getComputedStyle(element)[name] === value
 */
var openColorPickerAndSelectColor = Task.async(function* (view, ruleIndex,
    propIndex, newRgba, expectedChange) {
  let ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  let propEditor = ruleEditor.rule.textProps[propIndex].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-colorswatch");
  let cPicker = view.tooltips.getTooltip("colorPicker");

  info("Opening the colorpicker by clicking the color swatch");
  let onColorPickerReady = cPicker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  yield simulateColorPickerChange(view, cPicker, newRgba, expectedChange);

  return {propEditor, swatch, cPicker};
});

/**
 * Open the cubicbezier popup for a given property in a given rule and
 * simulate a curve change. Optionally wait for a given element in the page to
 * have its style changed as a result.
 *
 * @param {RuleView} view
 *        The related rule view instance
 * @param {Number} ruleIndex
 *        Which rule to target in the rule view
 * @param {Number} propIndex
 *        Which property to target in the rule
 * @param {Array} coords
 *        The new coordinates to be used, e.g. [0.1, 2, 0.9, -1]
 * @param {Object} expectedChange
 *        Optional object that needs the following props:
 *          - {String} selector The selector to the element in the page that
 *            will have its style changed.
 *          - {String} name The style name that will be changed
 *          - {String} value The expected style value
 * The style will be checked like so: getComputedStyle(element)[name] === value
 */
var openCubicBezierAndChangeCoords = Task.async(function* (view, ruleIndex,
    propIndex, coords, expectedChange) {
  let ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  let propEditor = ruleEditor.rule.textProps[propIndex].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-bezierswatch");
  let bezierTooltip = view.tooltips.getTooltip("cubicBezier");

  info("Opening the cubicBezier by clicking the swatch");
  let onBezierWidgetReady = bezierTooltip.once("ready");
  swatch.click();
  yield onBezierWidgetReady;

  let widget = yield bezierTooltip.widget;

  info("Simulating a change of curve in the widget");
  let onRuleViewChanged = view.once("ruleview-changed");
  widget.coordinates = coords;
  yield onRuleViewChanged;

  if (expectedChange) {
    info("Waiting for the style to be applied on the page");
    let {selector, name, value} = expectedChange;
    yield waitForComputedStyleProperty(selector, null, name, value);
  }

  return {propEditor, swatch, bezierTooltip};
});

/**
 * Simulate adding a new property in an existing rule in the rule-view.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {Number} ruleIndex
 *        The index of the rule to use. Note that if ruleIndex is 0, you might
 *        want to also listen to markupmutation events in your test since
 *        that's going to change the style attribute of the selected node.
 * @param {String} name
 *        The name for the new property
 * @param {String} value
 *        The value for the new property
 * @param {String} commitValueWith
 *        Which key should be used to commit the new value. VK_RETURN is used by
 *        default, but tests might want to use another key to test cancelling
 *        for exemple.
 * @param {Boolean} blurNewProperty
 *        After the new value has been added, a new property would have been
 *        focused. This parameter is true by default, and that causes the new
 *        property to be blurred. Set to false if you don't want this.
 * @return {TextProperty} The instance of the TextProperty that was added
 */
var addProperty = Task.async(function* (view, ruleIndex, name, value,
                                        commitValueWith = "VK_RETURN",
                                        blurNewProperty = true) {
  info("Adding new property " + name + ":" + value + " to rule " + ruleIndex);

  let ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  let editor = yield focusNewRuleViewProperty(ruleEditor);
  let numOfProps = ruleEditor.rule.textProps.length;

  info("Adding name " + name);
  editor.input.value = name;
  let onNameAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onNameAdded;

  // Focus has moved to the value inplace-editor automatically.
  editor = inplaceEditor(view.styleDocument.activeElement);
  let textProps = ruleEditor.rule.textProps;
  let textProp = textProps[textProps.length - 1];

  is(ruleEditor.rule.textProps.length, numOfProps + 1,
     "A new test property was added");
  is(editor, inplaceEditor(textProp.editor.valueSpan),
     "The inplace editor appeared for the value");

  info("Adding value " + value);
  // Setting the input value schedules a preview to be shown in 10ms which
  // triggers a ruleview-changed event (see bug 1209295).
  let onPreview = view.once("ruleview-changed");
  editor.input.value = value;
  view.debounce.flush();
  yield onPreview;

  let onValueAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey(commitValueWith, {}, view.styleWindow);
  yield onValueAdded;

  if (blurNewProperty) {
    view.styleDocument.activeElement.blur();
  }

  return textProp;
});

/**
 * Simulate changing the value of a property in a rule in the rule-view.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {TextProperty} textProp
 *        The instance of the TextProperty to be changed
 * @param {String} value
 *        The new value to be used. If null is passed, then the value will be
 *        deleted
 * @param {Boolean} blurNewProperty
 *        After the value has been changed, a new property would have been
 *        focused. This parameter is true by default, and that causes the new
 *        property to be blurred. Set to false if you don't want this.
 */
var setProperty = Task.async(function* (view, textProp, value,
                                        blurNewProperty = true) {
  yield focusEditableField(view, textProp.editor.valueSpan);

  let onPreview = view.once("ruleview-changed");
  if (value === null) {
    EventUtils.synthesizeKey("VK_DELETE", {}, view.styleWindow);
  } else {
    EventUtils.sendString(value, view.styleWindow);
  }
  view.debounce.flush();
  yield onPreview;

  let onValueDone = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueDone;

  if (blurNewProperty) {
    view.styleDocument.activeElement.blur();
  }
});

/**
 * Simulate removing a property from an existing rule in the rule-view.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {TextProperty} textProp
 *        The instance of the TextProperty to be removed
 * @param {Boolean} blurNewProperty
 *        After the property has been removed, a new property would have been
 *        focused. This parameter is true by default, and that causes the new
 *        property to be blurred. Set to false if you don't want this.
 */
var removeProperty = Task.async(function* (view, textProp,
                                           blurNewProperty = true) {
  yield focusEditableField(view, textProp.editor.nameSpan);

  let onModifications = view.once("ruleview-changed");
  info("Deleting the property name now");
  EventUtils.synthesizeKey("VK_DELETE", {}, view.styleWindow);
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onModifications;

  if (blurNewProperty) {
    view.styleDocument.activeElement.blur();
  }
});

/**
 * Simulate clicking the enable/disable checkbox next to a property in a rule.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {TextProperty} textProp
 *        The instance of the TextProperty to be enabled/disabled
 */
var togglePropStatus = Task.async(function* (view, textProp) {
  let onRuleViewRefreshed = view.once("ruleview-changed");
  textProp.editor.enable.click();
  yield onRuleViewRefreshed;
});

/**
 * Reload the current page and wait for the inspector to be initialized after
 * the navigation
 *
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {TestActor} testActor
 *        The current instance of the TestActor
 */
function* reloadPage(inspector, testActor) {
  let onNewRoot = inspector.once("new-root");
  yield testActor.reload();
  yield onNewRoot;
  yield inspector.markup._waitForChildren();
}

/**
 * Create a new rule by clicking on the "add rule" button.
 * This will leave the selector inplace-editor active.
 *
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @return a promise that resolves after the rule has been added
 */
function* addNewRule(inspector, view) {
  info("Adding the new rule using the button");
  view.addRuleButton.click();

  info("Waiting for rule view to change");
  yield view.once("ruleview-changed");
}

/**
 * Create a new rule by clicking on the "add rule" button, dismiss the editor field and
 * verify that the selector is correct.
 *
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {String} expectedSelector
 *        The value we expect the selector to have
 * @param {Number} expectedIndex
 *        The index we expect the rule to have in the rule-view
 * @return a promise that resolves after the rule has been added
 */
function* addNewRuleAndDismissEditor(inspector, view, expectedSelector, expectedIndex) {
  yield addNewRule(inspector, view);

  info("Getting the new rule at index " + expectedIndex);
  let ruleEditor = getRuleViewRuleEditor(view, expectedIndex);
  let editor = ruleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, expectedSelector,
     "The editor for the new selector has the correct value: " + expectedSelector);

  info("Pressing escape to leave the editor");
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  is(ruleEditor.selectorText.textContent, expectedSelector,
     "The new selector has the correct text: " + expectedSelector);
}

/**
 * Simulate a sequence of non-character keys (return, escape, tab) and wait for
 * a given element to receive the focus.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {DOMNode} element
 *        The element that should be focused
 * @param {Array} keys
 *        Array of non-character keys, the part that comes after "DOM_VK_" eg.
 *        "RETURN", "ESCAPE"
 * @return a promise that resolves after the element received the focus
 */
function* sendKeysAndWaitForFocus(view, element, keys) {
  let onFocus = once(element, "focus", true);
  for (let key of keys) {
    EventUtils.sendKey(key, view.styleWindow);
  }
  yield onFocus;
}

/**
 * Wait for a markupmutation event on the inspector that is for a style modification.
 * @param {InspectorPanel} inspector
 * @return {Promise}
 */
function waitForStyleModification(inspector) {
  return new Promise(function (resolve) {
    function checkForStyleModification(name, mutations) {
      for (let mutation of mutations) {
        if (mutation.type === "attributes" && mutation.attributeName === "style") {
          inspector.off("markupmutation", checkForStyleModification);
          resolve();
          return;
        }
      }
    }
    inspector.on("markupmutation", checkForStyleModification);
  });
}

/**
 * Click on the selector icon
 * @param {DOMNode} icon
 * @param {CSSRuleView} view
 */
function* clickSelectorIcon(icon, view) {
  let onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  yield onToggled;
}

/**
 * Toggle one of the checkboxes inside the class-panel. Resolved after the DOM mutation
 * has been recorded.
 * @param {CssRuleView} view The rule-view instance.
 * @param {String} name The class name to find the checkbox.
 */
function* toggleClassPanelCheckBox(view, name) {
  info(`Clicking on checkbox for class ${name}`);
  const checkBox = [...view.classPanel.querySelectorAll("[type=checkbox]")].find(box => {
    return box.dataset.name === name;
  });

  const onMutation = view.inspector.once("markupmutation");
  checkBox.click();
  info("Waiting for a markupmutation as a result of toggling this class");
  yield onMutation;
}

/**
 * Verify the content of the class-panel.
 * @param {CssRuleView} view The rule-view isntance
 * @param {Array} classes The list of expected classes. Each item in this array is an
 * object with the following properties: {name: {String}, state: {Boolean}}
 */
function checkClassPanelContent(view, classes) {
  const checkBoxNodeList = view.classPanel.querySelectorAll("[type=checkbox]");
  is(checkBoxNodeList.length, classes.length,
     "The panel contains the expected number of checkboxes");

  for (let i = 0; i < classes.length; i++) {
    is(checkBoxNodeList[i].dataset.name, classes[i].name,
       `Checkbox ${i} has the right class name`);
    is(checkBoxNodeList[i].checked, classes[i].state,
       `Checkbox ${i} has the right state`);
  }
}
