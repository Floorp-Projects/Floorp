/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/telemetry-test-helpers.js */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
});

var {
  getInplaceEditorForSpan: inplaceEditor,
} = require("devtools/client/shared/inplace-editor");

const {
  COMPATIBILITY_TOOLTIP_MESSAGE,
} = require("devtools/client/inspector/rules/constants");

const ROOT_TEST_DIR = getRootDirectory(gTestPath);

const STYLE_INSPECTOR_L10N = new LocalizationHelper(
  "devtools/shared/locales/styleinspector.properties"
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
});

/**
 * When a tooltip is closed, this ends up "commiting" the value changed within
 * the tooltip (e.g. the color in case of a colorpicker) which, in turn, ends up
 * setting the value of the corresponding css property in the rule-view.
 * Use this function to close the tooltip and make sure the test waits for the
 * ruleview-changed event.
 * @param {SwatchBasedEditorTooltip} editorTooltip
 * @param {CSSRuleView} view
 */
async function hideTooltipAndWaitForRuleViewChanged(editorTooltip, view) {
  const onModified = view.once("ruleview-changed");
  const onHidden = editorTooltip.tooltip.once("hidden");
  editorTooltip.hide();
  await onModified;
  await onHidden;
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
var waitForSuccess = async function(validatorFn, desc = "untitled") {
  let i = 0;
  while (true) {
    info("Checking: " + desc);
    if (await validatorFn()) {
      ok(true, "Success: " + desc);
      break;
    }
    i++;
    if (i > 10) {
      ok(false, "Failure: " + desc);
      break;
    }
    await new Promise(r => setTimeout(r, 200));
  }
};

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
var simulateColorPickerChange = async function(
  ruleView,
  colorPicker,
  newRgba,
  expectedChange
) {
  let onComputedStyleChanged;
  if (expectedChange) {
    const { selector, name, value } = expectedChange;
    onComputedStyleChanged = waitForComputedStyleProperty(
      selector,
      null,
      name,
      value
    );
  }
  const onRuleViewChanged = ruleView.once("ruleview-changed");
  info("Getting the spectrum colorpicker object");
  const spectrum = colorPicker.spectrum;
  info("Setting the new color");
  spectrum.rgb = newRgba;
  info("Applying the change");
  spectrum.updateUI();
  spectrum.onChange();
  info("Waiting for rule-view to update");
  await onRuleViewChanged;

  if (expectedChange) {
    info("Waiting for the style to be applied on the page");
    await onComputedStyleChanged;
  }
};

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
var openColorPickerAndSelectColor = async function(
  view,
  ruleIndex,
  propIndex,
  newRgba,
  expectedChange
) {
  const ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  const propEditor = ruleEditor.rule.textProps[propIndex].editor;
  const swatch = propEditor.valueSpan.querySelector(".ruleview-colorswatch");
  const cPicker = view.tooltips.getTooltip("colorPicker");

  info("Opening the colorpicker by clicking the color swatch");
  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(view, cPicker, newRgba, expectedChange);

  return { propEditor, swatch, cPicker };
};

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
var openCubicBezierAndChangeCoords = async function(
  view,
  ruleIndex,
  propIndex,
  coords,
  expectedChange
) {
  const ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  const propEditor = ruleEditor.rule.textProps[propIndex].editor;
  const swatch = propEditor.valueSpan.querySelector(".ruleview-bezierswatch");
  const bezierTooltip = view.tooltips.getTooltip("cubicBezier");

  info("Opening the cubicBezier by clicking the swatch");
  const onBezierWidgetReady = bezierTooltip.once("ready");
  swatch.click();
  await onBezierWidgetReady;

  const widget = await bezierTooltip.widget;

  info("Simulating a change of curve in the widget");
  const onRuleViewChanged = view.once("ruleview-changed");
  widget.coordinates = coords;
  await onRuleViewChanged;

  if (expectedChange) {
    info("Waiting for the style to be applied on the page");
    const { selector, name, value } = expectedChange;
    await waitForComputedStyleProperty(selector, null, name, value);
  }

  return { propEditor, swatch, bezierTooltip };
};

/**
 * Simulate adding a new property in an existing rule in the rule-view.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {Number} ruleIndex
 *        The index of the rule to use.
 * @param {String} name
 *        The name for the new property
 * @param {String} value
 *        The value for the new property
 * @param {Object=} options
 * @param {String=} options.commitValueWith
 *        Which key should be used to commit the new value. VK_RETURN is used by
 *        default, but tests might want to use another key to test cancelling
 *        for exemple.
 * @param {Boolean=} options.blurNewProperty
 *        After the new value has been added, a new property would have been
 *        focused. This parameter is true by default, and that causes the new
 *        property to be blurred. Set to false if you don't want this.
 * @return {TextProperty} The instance of the TextProperty that was added
 */
var addProperty = async function(
  view,
  ruleIndex,
  name,
  value,
  { commitValueWith = "VK_RETURN", blurNewProperty = true } = {}
) {
  info("Adding new property " + name + ":" + value + " to rule " + ruleIndex);

  const ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  let editor = await focusNewRuleViewProperty(ruleEditor);
  const numOfProps = ruleEditor.rule.textProps.length;

  const onMutations = new Promise(r => {
    // If the rule index is 0, then we are updating the rule for the "element"
    // selector in the rule view.
    // This rule is actually updating the style attribute of the element, and
    // therefore we can expect mutations.
    // For any other rule index, no mutation should be created, we can resolve
    // immediately.
    if (ruleIndex !== 0) {
      r();
    }

    // Use CSS.escape for the name in order to match the logic at
    // devtools/client/fronts/inspector/rule-rewriter.js
    // This leads to odd values in the style attribute and might change in the
    // future. See https://bugzilla.mozilla.org/show_bug.cgi?id=1765943
    const expectedAttributeValue = `${CSS.escape(name)}: ${value}`;
    view.inspector.walker.on("mutations", function onWalkerMutations(
      mutations
    ) {
      // Wait until we receive a mutation which updates the style attribute
      // with the expected value.
      const receivedLastMutation = mutations.some(
        mut =>
          mut.attributeName === "style" &&
          mut.newValue.includes(expectedAttributeValue)
      );
      if (receivedLastMutation) {
        view.inspector.walker.off("mutations", onWalkerMutations);
        r();
      }
    });
  });

  info("Adding name " + name);
  editor.input.value = name;
  const onNameAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onNameAdded;

  // Focus has moved to the value inplace-editor automatically.
  editor = inplaceEditor(view.styleDocument.activeElement);
  const textProps = ruleEditor.rule.textProps;
  const textProp = textProps[textProps.length - 1];

  is(
    ruleEditor.rule.textProps.length,
    numOfProps + 1,
    "A new test property was added"
  );
  is(
    editor,
    inplaceEditor(textProp.editor.valueSpan),
    "The inplace editor appeared for the value"
  );

  info("Adding value " + value);
  // Setting the input value schedules a preview to be shown in 10ms which
  // triggers a ruleview-changed event (see bug 1209295).
  const onPreview = view.once("ruleview-changed");
  editor.input.value = value;
  view.debounce.flush();
  await onPreview;

  const onValueAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey(commitValueWith, {}, view.styleWindow);
  await onValueAdded;

  info(
    "Waiting for DOM mutations in case the property was added to the element style"
  );
  await onMutations;

  if (blurNewProperty) {
    view.styleDocument.activeElement.blur();
  }

  return textProp;
};

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
 * @param {Object} options
 * @param {Boolean} options.blurNewProperty
 *        After the value has been changed, a new property would have been
 *        focused. This parameter is true by default, and that causes the new
 *        property to be blurred. Set to false if you don't want this.
 * @param {number} options.flushCount
 *        The ruleview uses a manual flush for tests only, and some properties are
 *        only updated after several flush. Allow tests to trigger several flushes
 *        if necessary. Defaults to 1.
 */
var setProperty = async function(
  view,
  textProp,
  value,
  { blurNewProperty = true, flushCount = 1 } = {}
) {
  info("Set property to: " + value);
  await focusEditableField(view, textProp.editor.valueSpan);

  const onPreview = view.once("ruleview-changed");
  if (value === null) {
    const onPopupOpened = once(view.popup, "popup-opened");
    EventUtils.synthesizeKey("VK_DELETE", {}, view.styleWindow);
    await onPopupOpened;
  } else {
    EventUtils.sendString(value, view.styleWindow);
  }

  info("Waiting for ruleview-changed after updating property");
  view.debounce.flush();
  flushCount--;

  while (flushCount > 0) {
    // Wait for some time before triggering a new flush to let new debounced
    // functions queue in-between.
    await wait(100);
    view.debounce.flush();
    flushCount--;
  }

  await onPreview;

  const onValueDone = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

  info("Waiting for another ruleview-changed after setting property");
  await onValueDone;

  if (blurNewProperty) {
    info("Force blur on the active element");
    view.styleDocument.activeElement.blur();
  }
};

/**
 * Change the name of a property in a rule in the rule-view.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel.
 * @param {TextProperty} textProp
 *        The instance of the TextProperty to be changed.
 * @param {String} name
 *        The new property name.
 */
var renameProperty = async function(view, textProp, name) {
  await focusEditableField(view, textProp.editor.nameSpan);

  const onNameDone = view.once("ruleview-changed");
  info(`Rename the property to ${name}`);
  EventUtils.sendString(name, view.styleWindow);
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  info("Wait for property name.");
  await onNameDone;
  // Renaming the property auto-advances the focus to the value input. Exiting without
  // committing will still fire a change event. @see TextPropertyEditor._onValueDone().
  // Wait for that event too before proceeding.
  const onValueDone = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  info("Wait for property value.");
  await onValueDone;
};

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
var removeProperty = async function(view, textProp, blurNewProperty = true) {
  await focusEditableField(view, textProp.editor.nameSpan);

  const onModifications = view.once("ruleview-changed");
  info("Deleting the property name now");
  EventUtils.synthesizeKey("VK_DELETE", {}, view.styleWindow);
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onModifications;

  if (blurNewProperty) {
    view.styleDocument.activeElement.blur();
  }
};

/**
 * Simulate clicking the enable/disable checkbox next to a property in a rule.
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {TextProperty} textProp
 *        The instance of the TextProperty to be enabled/disabled
 */
var togglePropStatus = async function(view, textProp) {
  const onRuleViewRefreshed = view.once("ruleview-changed");
  textProp.editor.enable.click();
  await onRuleViewRefreshed;
};

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
async function addNewRule(inspector, view) {
  info("Adding the new rule using the button");
  view.addRuleButton.click();

  info("Waiting for rule view to change");
  await view.once("ruleview-changed");
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
async function addNewRuleAndDismissEditor(
  inspector,
  view,
  expectedSelector,
  expectedIndex
) {
  await addNewRule(inspector, view);

  info("Getting the new rule at index " + expectedIndex);
  const ruleEditor = getRuleViewRuleEditor(view, expectedIndex);
  const editor = ruleEditor.selectorText.ownerDocument.activeElement;
  is(
    editor.value,
    expectedSelector,
    "The editor for the new selector has the correct value: " + expectedSelector
  );

  info("Pressing escape to leave the editor");
  EventUtils.synthesizeKey("KEY_Escape");

  is(
    ruleEditor.selectorText.textContent,
    expectedSelector,
    "The new selector has the correct text: " + expectedSelector
  );
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
async function sendKeysAndWaitForFocus(view, element, keys) {
  const onFocus = once(element, "focus", true);
  for (const key of keys) {
    EventUtils.sendKey(key, view.styleWindow);
  }
  await onFocus;
}

/**
 * Wait for a markupmutation event on the inspector that is for a style modification.
 * @param {InspectorPanel} inspector
 * @return {Promise}
 */
function waitForStyleModification(inspector) {
  return new Promise(function(resolve) {
    function checkForStyleModification(mutations) {
      for (const mutation of mutations) {
        if (
          mutation.type === "attributes" &&
          mutation.attributeName === "style"
        ) {
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
 * Click on the icon next to the selector of a CSS rule in the Rules view
 * to toggle the selector highlighter. If a selector highlighter is not already visible
 * for the given selector, wait for it to be shown. Otherwise, wait for it to be hidden.
 *
 * @param {CssRuleView} view
 *        The instance of the Rules view
 * @param {String} selectorText
 *        The selector of the CSS rule to look for
 * @param {Number} index
 *        If there are more CSS rules with the same selector, use this index
 *        to determine which one should be retrieved. Defaults to 0 (first)
 */
async function clickSelectorIcon(view, selectorText, index = 0) {
  const { inspector } = view;
  const rule = getRuleViewRule(view, selectorText, index);

  // The icon element is created in response to an async operation.
  // Wait until the expected node shows up in the DOM. (Bug 1664511)
  info(`Waiting for icon to be available for selector: ${selectorText}`);
  const icon = await waitFor(() => {
    return rule.querySelector(".js-toggle-selector-highlighter");
  });

  // Grab the actual selector associated with the matched icon.
  // For inline styles, the CSS rule with the "element" selector actually points to
  // a generated unique selector, for example: "div:nth-child(1)".
  // The selector highlighter is invoked with this unique selector.
  // Continuing to use selectorText ("element") would fail some of the checks below.
  const selector = icon.dataset.selector;

  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  // If there is an active selector highlighter, get its configuration options.
  // Will be undefined if there isn't an active selector highlighter.
  const options = inspector.highlighters.getOptionsForActiveHighlighter(
    inspector.highlighters.TYPES.SELECTOR
  );

  // If there is already a highlighter visible for this selector,
  // wait for hidden event. Otherwise, wait for shown event.
  const waitForEvent =
    options?.selector === selector
      ? waitForHighlighterTypeHidden(inspector.highlighters.TYPES.SELECTOR)
      : waitForHighlighterTypeShown(inspector.highlighters.TYPES.SELECTOR);

  // Boolean flag whether we waited for a highlighter shown event
  const waitedForShown = options?.selector !== selector;

  info(`Click the icon for selector: ${selectorText}`);
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);

  // Promise resolves with event data from either highlighter shown or hidden event.
  const data = await waitForEvent;
  return { ...data, isShown: waitedForShown };
}
/**
 * Toggle one of the checkboxes inside the class-panel. Resolved after the DOM mutation
 * has been recorded.
 * @param {CssRuleView} view The rule-view instance.
 * @param {String} name The class name to find the checkbox.
 */
async function toggleClassPanelCheckBox(view, name) {
  info(`Clicking on checkbox for class ${name}`);
  const checkBox = [
    ...view.classPanel.querySelectorAll("[type=checkbox]"),
  ].find(box => {
    return box.dataset.name === name;
  });

  const onMutation = view.inspector.once("markupmutation");
  checkBox.click();
  info("Waiting for a markupmutation as a result of toggling this class");
  await onMutation;
}

/**
 * Verify the content of the class-panel.
 * @param {CssRuleView} view The rule-view instance
 * @param {Array} classes The list of expected classes. Each item in this array is an
 * object with the following properties: {name: {String}, state: {Boolean}}
 */
function checkClassPanelContent(view, classes) {
  const checkBoxNodeList = view.classPanel.querySelectorAll("[type=checkbox]");
  is(
    checkBoxNodeList.length,
    classes.length,
    "The panel contains the expected number of checkboxes"
  );

  for (let i = 0; i < classes.length; i++) {
    is(
      checkBoxNodeList[i].dataset.name,
      classes[i].name,
      `Checkbox ${i} has the right class name`
    );
    is(
      checkBoxNodeList[i].checked,
      classes[i].state,
      `Checkbox ${i} has the right state`
    );
  }
}

/**
 * Opens the eyedropper from the colorpicker tooltip
 * by selecting the colorpicker and then selecting the eyedropper icon
 * @param {view} ruleView
 * @param {swatch} color swatch of a particular property
 */
async function openEyedropper(view, swatch) {
  const tooltip = view.tooltips.getTooltip("colorPicker").tooltip;

  info("Click on the swatch");
  const onColorPickerReady = view.tooltips
    .getTooltip("colorPicker")
    .once("ready");
  EventUtils.synthesizeMouseAtCenter(swatch, {}, swatch.ownerGlobal);
  await onColorPickerReady;

  const dropperButton = tooltip.container.querySelector("#eyedropper-button");

  info("Click on the eyedropper icon");
  const onOpened = tooltip.once("eyedropper-opened");
  dropperButton.click();
  await onOpened;
}

/**
 * Gets a set of declarations for a rule index.
 *
 * @param {ruleView} view
 *        The rule-view instance.
 * @param {Number} ruleIndex
 *        The index we expect the rule to have in the rule-view.
 * @param {boolean} addCompatibilityData
 *        Optional argument to add compatibility dat with the property data
 *
 * @returns A Promise that resolves with a Map containing stringified property declarations e.g.
 *          [
 *            {
 *              "color:red":
 *                {
 *                  propertyName: "color",
 *                  propertyValue: "red",
 *                  warning: "This won't work",
 *                  used: true,
 *                  compatibilityData: {
 *                    isCompatible: true,
 *                  },
 *                }
 *            },
 *            ...
 *          ]
 */
async function getPropertiesForRuleIndex(
  view,
  ruleIndex,
  addCompatibilityData = false
) {
  const declaration = new Map();
  const ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  for (const currProp of ruleEditor.rule.textProps) {
    const icon = currProp.editor.unusedState;
    const unused = currProp.editor.element.classList.contains("unused");

    let compatibilityData;
    let compatibilityIcon;
    if (addCompatibilityData) {
      compatibilityData = await currProp.isCompatible();
      compatibilityIcon = currProp.editor.compatibilityState;
    }

    declaration.set(`${currProp.name}:${currProp.value}`, {
      propertyName: currProp.name,
      propertyValue: currProp.value,
      icon: icon,
      data: currProp.isUsed(),
      warning: unused,
      used: !unused,
      ...(addCompatibilityData
        ? {
            compatibilityData,
            compatibilityIcon,
          }
        : {}),
    });
  }

  return declaration;
}

/**
 * Toggle a declaration disabled or enabled.
 *
 * @param {ruleView} view
 *        The rule-view instance
 * @param {Number} ruleIndex
 *        The index of the CSS rule where we can find the declaration to be
 *        toggled.
 * @param {Object} declaration
 *        An object representing the declaration e.g. { color: "red" }.
 */
async function toggleDeclaration(view, ruleIndex, declaration) {
  const textProp = getTextProperty(view, ruleIndex, declaration);
  const [[name, value]] = Object.entries(declaration);
  const dec = `${name}:${value}`;
  ok(textProp, `Declaration "${dec}" found`);

  const newStatus = textProp.enabled ? "disabled" : "enabled";
  info(`Toggling declaration "${dec}" of rule ${ruleIndex} to ${newStatus}`);

  await togglePropStatus(view, textProp);
  info("Toggled successfully.");
}

/**
 * Update a declaration from a CSS rule in the Rules view
 * by changing its property name, property value or both.
 *
 * @param {RuleView} view
 *        Instance of RuleView.
 * @param {Number} ruleIndex
 *        The index of the CSS rule where to find the declaration.
 * @param {Object} declaration
 *        An object representing the target declaration e.g. { color: red }.
 * @param {Object} newDeclaration
 *        An object representing the desired updated declaration e.g. { display: none }.
 */
async function updateDeclaration(
  view,
  ruleIndex,
  declaration,
  newDeclaration = {}
) {
  const textProp = getTextProperty(view, ruleIndex, declaration);
  const [[name, value]] = Object.entries(declaration);
  const [[newName, newValue]] = Object.entries(newDeclaration);

  if (newName && name !== newName) {
    info(
      `Updating declaration ${name}:${value};
      Changing ${name} to ${newName}`
    );
    await renameProperty(view, textProp, newName);
  }

  if (newValue && value !== newValue) {
    info(
      `Updating declaration ${name}:${value};
      Changing ${value} to ${newValue}`
    );
    await setProperty(view, textProp, newValue);
  }
}

/**
 * Get the TextProperty instance corresponding to a CSS declaration
 * from a CSS rule in the Rules view.
 *
 * @param  {RuleView} view
 *         Instance of RuleView.
 * @param  {Number} ruleIndex
 *         The index of the CSS rule where to find the declaration.
 * @param  {Object} declaration
 *         An object representing the target declaration e.g. { color: red }.
 *         The first TextProperty instance which matches will be returned.
 * @return {TextProperty}
 */
function getTextProperty(view, ruleIndex, declaration) {
  const ruleEditor = getRuleViewRuleEditor(view, ruleIndex);
  const [[name, value]] = Object.entries(declaration);
  const textProp = ruleEditor.rule.textProps.find(prop => {
    return prop.name === name && prop.value === value;
  });

  if (!textProp) {
    throw Error(
      `Declaration ${name}:${value} not found on rule at index ${ruleIndex}`
    );
  }

  return textProp;
}

/**
 * Check whether the given CSS declaration is compatible or not
 *
 * @param {ruleView} view
 *        The rule-view instance.
 * @param {Number} ruleIndex
 *        The index we expect the rule to have in the rule-view.
 * @param {Object} declaration
 *        An object representing the declaration e.g. { color: "red" }.
 * @param {string | undefined} expected
 *        Expected message ID for the given incompatible property.
 * If the expected message is not specified (undefined), the given declaration
 * is inferred as cross-browser compatible and is tested for same.
 */
async function checkDeclarationCompatibility(
  view,
  ruleIndex,
  declaration,
  expected
) {
  const declarations = await getPropertiesForRuleIndex(view, ruleIndex, true);
  const [[name, value]] = Object.entries(declaration);
  const dec = `${name}:${value}`;
  const { compatibilityData } = declarations.get(dec);

  is(
    !expected,
    compatibilityData.isCompatible,
    `"${dec}" has the correct compatibility status in the payload`
  );

  is(compatibilityData.msgId, expected, `"${dec}" has expected message ID`);

  if (expected) {
    await checkInteractiveTooltip(
      view,
      "compatibility-tooltip",
      ruleIndex,
      declaration
    );
  }
}

/**
 * Check that a declaration is marked inactive and that it has the expected
 * warning.
 *
 * @param {ruleView} view
 *        The rule-view instance.
 * @param {Number} ruleIndex
 *        The index we expect the rule to have in the rule-view.
 * @param {Object} declaration
 *        An object representing the declaration e.g. { color: "red" }.
 */
async function checkDeclarationIsInactive(view, ruleIndex, declaration) {
  const declarations = await getPropertiesForRuleIndex(view, ruleIndex);
  const [[name, value]] = Object.entries(declaration);
  const dec = `${name}:${value}`;
  const { used, warning } = declarations.get(dec);

  ok(!used, `"${dec}" is inactive`);
  ok(warning, `"${dec}" has a warning`);

  await checkInteractiveTooltip(
    view,
    "inactive-css-tooltip",
    ruleIndex,
    declaration
  );
}

/**
 * Check that a declaration is marked active.
 *
 * @param {ruleView} view
 *        The rule-view instance.
 * @param {Number} ruleIndex
 *        The index we expect the rule to have in the rule-view.
 * @param {Object} declaration
 *        An object representing the declaration e.g. { color: "red" }.
 */
async function checkDeclarationIsActive(view, ruleIndex, declaration) {
  const declarations = await getPropertiesForRuleIndex(view, ruleIndex);
  const [[name, value]] = Object.entries(declaration);
  const dec = `${name}:${value}`;
  const { used, warning } = declarations.get(dec);

  ok(used, `${dec} is active`);
  ok(!warning, `${dec} has no warning`);
}

/**
 * Check that a tooltip contains the correct value.
 *
 * @param {ruleView} view
 *        The rule-view instance.
 *  @param {string} type
 *        The interactive tooltip type being tested.
 * @param {Number} ruleIndex
 *        The index we expect the rule to have in the rule-view.
 * @param {Object} declaration
 *        An object representing the declaration e.g. { color: "red" }.
 */
async function checkInteractiveTooltip(view, type, ruleIndex, declaration) {
  // Get the declaration
  const declarations = await getPropertiesForRuleIndex(
    view,
    ruleIndex,
    type === "compatibility-tooltip"
  );
  const [[name, value]] = Object.entries(declaration);
  const dec = `${name}:${value}`;

  // Get the relevant icon and tooltip payload data
  let icon;
  let data;
  if (type === "inactive-css-tooltip") {
    ({ icon, data } = declarations.get(dec));
  } else {
    const { compatibilityIcon, compatibilityData } = declarations.get(dec);
    icon = compatibilityIcon;
    data = compatibilityData;
  }

  // Get the tooltip.
  const tooltip = view.tooltips.getTooltip("interactiveTooltip");

  // Get the necessary tooltip helper to fetch the Fluent template.
  let tooltipHelper;
  if (type === "inactive-css-tooltip") {
    tooltipHelper = view.tooltips.inactiveCssTooltipHelper;
  } else {
    tooltipHelper = view.tooltips.compatibilityTooltipHelper;
  }

  // Get the HTML template.
  const template = tooltipHelper.getTemplate(data, tooltip);

  // Translate the template using Fluent.
  const { doc } = tooltip;
  await doc.l10n.translateFragment(template);

  // Get the expected HTML content of the now translated template.
  const expected = template.firstElementChild.outerHTML;

  // Show the tooltip for the correct icon.
  const onTooltipReady = tooltip.once("shown");
  await view.tooltips.onInteractiveTooltipTargetHover(icon);
  tooltip.show(icon);
  await onTooltipReady;

  // Get the tooltip's actual HTML content.
  const actual = tooltip.panel.firstElementChild.outerHTML;

  // Hide the tooltip.
  const onTooltipHidden = tooltip.once("hidden");
  tooltip.hide();
  await onTooltipHidden;

  // Finally, check the values.
  is(actual, expected, "Tooltip contains the correct value.");
}

/**
 * CSS compatibility test runner.
 *
 * @param {ruleView} view
 *        The rule-view instance.
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox.
 * @param {Array} tests
 *        An array of test object for this method to consume e.g.
 *          [
 *            {
 *              selector: "#flex-item",
 *              rules: [
 *                // Rule Index: 0
 *                {
 *                  // If the object doesn't include the "expected"
 *                  // key, we consider the declaration as
 *                  // cross-browser compatible and test for same
 *                  color: { value: "green" },
 *                },
 *                // Rule Index: 1
 *                {
 *                  cursor:
 *                  {
 *                    value: "grab",
 *                    expected: INCOMPATIBILITY_TOOLTIP_MESSAGE.default,
 *                  },
 *                },
 *              ],
 *            },
 *            ...
 *          ]
 */
async function runCSSCompatibilityTests(view, inspector, tests) {
  for (const test of tests) {
    if (test.selector) {
      await selectNode(test.selector, inspector);
    }

    for (const [ruleIndex, rules] of test.rules.entries()) {
      for (const rule in rules) {
        await checkDeclarationCompatibility(
          view,
          ruleIndex,
          {
            [rule]: rules[rule].value,
          },
          rules[rule].expected
        );
      }
    }
  }
}

/**
 * Inactive CSS test runner.
 *
 * @param {ruleView} view
 *        The rule-view instance.
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox.
 * @param {Array} tests
 *        An array of test object for this method to consume e.g.
 *          [
 *            {
 *              selector: "#flex-item",
 *              activeDeclarations: [
 *                {
 *                  declarations: {
 *                    "order": "2",
 *                  },
 *                  ruleIndex: 0,
 *                },
 *                {
 *                  declarations: {
 *                    "flex-basis": "auto",
 *                    "flex-grow": "1",
 *                    "flex-shrink": "1",
 *                  },
 *                  ruleIndex: 1,
 *                },
 *              ],
 *              inactiveDeclarations: [
 *                {
 *                  declaration: {
 *                    "flex-direction": "row",
 *                  },
 *                  ruleIndex: 1,
 *                },
 *              ],
 *            },
 *            ...
 *          ]
 */
async function runInactiveCSSTests(view, inspector, tests) {
  for (const test of tests) {
    if (test.selector) {
      await selectNode(test.selector, inspector);
    }

    if (test.activeDeclarations) {
      info("Checking whether declarations are marked as used.");

      for (const activeDeclarations of test.activeDeclarations) {
        for (const [name, value] of Object.entries(
          activeDeclarations.declarations
        )) {
          await checkDeclarationIsActive(view, activeDeclarations.ruleIndex, {
            [name]: value,
          });
        }
      }
    }

    if (test.inactiveDeclarations) {
      info("Checking that declarations are unused and have a warning.");

      for (const inactiveDeclaration of test.inactiveDeclarations) {
        await checkDeclarationIsInactive(
          view,
          inactiveDeclaration.ruleIndex,
          inactiveDeclaration.declaration
        );
      }
    }
  }
}

/**
 * Return the checkbox element from the Rules view corresponding
 * to the given pseudo-class.
 *
 * @param  {Object} view
 *         Instance of RuleView.
 * @param  {String} pseudo
 *         Pseudo-class, like :hover, :active, :focus, etc.
 * @return {HTMLElement}
 */
function getPseudoClassCheckbox(view, pseudo) {
  return view.pseudoClassCheckboxes.filter(
    checkbox => checkbox.value === pseudo
  )[0];
}

/**
 * Check that the CSS variable output has the expected class name and data attribute.
 *
 * @param {RulesView} view
 *        The RulesView instance.
 * @param {String} selector
 *        Selector name for a rule. (e.g. "div", "div::before" and ".sample" etc);
 * @param {String} propertyName
 *        Property name (e.g. "color" and "padding-top" etc);
 * @param {String} expectedClassName
 *        The class name the variable should have.
 * @param {String} expectedDatasetValue
 *        The variable data attribute value.
 */
function checkCSSVariableOutput(
  view,
  selector,
  propertyName,
  expectedClassName,
  expectedDatasetValue
) {
  const target = getRuleViewProperty(
    view,
    selector,
    propertyName
  ).valueSpan.querySelector(`.${expectedClassName}`);

  ok(target, "The target element should exist");
  is(target.dataset.variable, expectedDatasetValue);
}

/**
 * Return specific rule ancestor data element (i.e. the one containing @layer / @media
 * information) from the Rules view
 *
 * @param {RulesView} view
 *        The RulesView instance.
 * @param {Number} ruleIndex
 * @returns {HTMLElement}
 */
function getRuleViewAncestorRulesDataElementByIndex(view, ruleIndex) {
  return view.styleDocument.querySelector(
    `.ruleview-rule:nth-of-type(${ruleIndex + 1}) .ruleview-rule-ancestor-data`
  );
}

/**
 * Return specific rule ancestor data text from the Rules view.
 * Will return something like "@layer topLayer\n@media screen\n@layer".
 *
 * @param {RulesView} view
 *        The RulesView instance.
 * @param {Number} ruleIndex
 * @returns {String}
 */
function getRuleViewAncestorRulesDataTextByIndex(view, ruleIndex) {
  return getRuleViewAncestorRulesDataElementByIndex(view, ruleIndex)?.innerText;
}
