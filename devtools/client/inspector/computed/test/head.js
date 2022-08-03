/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
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

/**
 * Dispatch the copy event on the given element
 */
function fireCopyEvent(element) {
  const evt = element.ownerDocument.createEvent("Event");
  evt.initEvent("copy", true, true);
  element.dispatchEvent(evt);
}

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
  for (const property of view.styleDocument.querySelectorAll(
    "#computed-container .computed-property-view"
  )) {
    const nameSpan = property.querySelector(".computed-property-name");
    const valueSpan = property.querySelector(".computed-property-value");

    if (nameSpan.firstChild.textContent === name) {
      prop = { nameSpan, valueSpan };
      break;
    }
  }
  return prop;
}

/**
 * Get an instance of PropertyView from the computed-view.
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {PropertyView}
 */
function getComputedViewPropertyView(view, name) {
  let propView;
  for (const propertyView of view.propertyViews) {
    if (propertyView._propertyInfo.name === name) {
      propView = propertyView;
      break;
    }
  }
  return propView;
}

/**
 * Get a reference to the computed-property-content element for a given property name in
 * the computed-view.
 * A computed-property-content element always follows (nextSibling) the property itself
 * and is only shown when the twisty icon is expanded on the property.
 * A computed-property-content element contains matched rules, with selectors,
 * properties, values and stylesheet links
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {Promise} A promise that resolves to the property matched rules
 * container
 */
var getComputedViewMatchedRules = async function(view, name) {
  let expander;
  let propertyContent;
  for (const property of view.styleDocument.querySelectorAll(
    "#computed-container .computed-property-view"
  )) {
    const nameSpan = property.querySelector(".computed-property-name");
    if (nameSpan.firstChild.textContent === name) {
      expander = property.querySelector(".computed-expandable");
      propertyContent = property.nextSibling;
      break;
    }
  }

  if (!expander.hasAttribute("open")) {
    // Need to expand the property
    const onExpand = view.inspector.once("computed-view-property-expanded");
    expander.click();
    await onExpand;
  }

  return propertyContent;
};

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
  return getComputedViewProperty(view, name, propertyName).valueSpan
    .textContent;
}

/**
 * Expand a given property, given its index in the current property list of
 * the computed view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {Number} index
 *        The index of the property to be expanded
 * @return a promise that resolves when the property has been expanded, or
 * rejects if the property was not found
 */
function expandComputedViewPropertyByIndex(view, index) {
  info("Expanding property " + index + " in the computed view");
  const expandos = view.styleDocument.querySelectorAll(".computed-expandable");
  if (!expandos.length || !expandos[index]) {
    return Promise.reject();
  }

  const onExpand = view.inspector.once("computed-view-property-expanded");
  expandos[index].click();
  return onExpand;
}

/**
 * Get a rule-link from the computed-view given its index
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {Number} index
 *        The index of the link to be retrieved
 * @return {DOMNode} The link at the given index, if one exists, null otherwise
 */
function getComputedViewLinkByIndex(view, index) {
  const links = view.styleDocument.querySelectorAll(
    ".rule-link .computed-link"
  );
  return links[index];
}

/**
 * Trigger the select all action in the computed view.
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 */
function selectAllText(view) {
  info("Selecting all the text");
  view.contextMenu._onSelectAll();
}

/**
 * Select all the text, copy it, and check the content in the clipboard.
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} expectedPattern
 *        A regular expression used to check the content of the clipboard
 */
async function copyAllAndCheckClipboard(view, expectedPattern) {
  selectAllText(view);
  const contentDoc = view.styleDocument;
  const prop = contentDoc.querySelector(
    "#computed-container .computed-property-view"
  );

  try {
    info("Trigger a copy event and wait for the clipboard content");
    await waitForClipboardPromise(
      () => fireCopyEvent(prop),
      () => checkClipboard(expectedPattern)
    );
  } catch (e) {
    failClipboardCheck(expectedPattern);
  }
}

/**
 * Select some text, copy it, and check the content in the clipboard.
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {Object} positions
 *        The start and end positions of the text to be selected. This must be an object
 *        like this:
 *        { start: {prop: 1, offset: 0}, end: {prop: 3, offset: 5} }
 * @param {String} expectedPattern
 *        A regular expression used to check the content of the clipboard
 */
async function copySomeTextAndCheckClipboard(view, positions, expectedPattern) {
  info("Testing selection copy");

  const contentDocument = view.styleDocument;
  const props = contentDocument.querySelectorAll(
    "#computed-container .computed-property-view"
  );

  info("Create the text selection range");
  const range = contentDocument.createRange();
  range.setStart(props[positions.start.prop], positions.start.offset);
  range.setEnd(props[positions.end.prop], positions.end.offset);
  contentDocument.defaultView.getSelection().addRange(range);

  try {
    info("Trigger a copy event and wait for the clipboard content");
    await waitForClipboardPromise(
      () => fireCopyEvent(props[0]),
      () => checkClipboard(expectedPattern)
    );
  } catch (e) {
    failClipboardCheck(expectedPattern);
  }
}

function checkClipboard(expectedPattern) {
  const actual = SpecialPowers.getClipboardData("text/unicode");
  const expectedRegExp = new RegExp(expectedPattern, "g");
  return expectedRegExp.test(actual);
}

function failClipboardCheck(expectedPattern) {
  // Format expected text for comparison
  const terminator = Services.appinfo.OS == "WINNT" ? "\r\n" : "\n";
  expectedPattern = expectedPattern.replace(/\[\\r\\n\][+*]/g, terminator);
  expectedPattern = expectedPattern.replace(/\\\(/g, "(");
  expectedPattern = expectedPattern.replace(/\\\)/g, ")");

  let actual = SpecialPowers.getClipboardData("text/unicode");

  // Trim the right hand side of our strings. This is because expectedPattern
  // accounts for windows sometimes adding a newline to our copied data.
  expectedPattern = expectedPattern.trimRight();
  actual = actual.trimRight();

  dump(
    "TEST-UNEXPECTED-FAIL | Clipboard text does not match expected ... " +
      "results (escaped for accurate comparison):\n"
  );
  info("Actual: " + escape(actual));
  info("Expected: " + escape(expectedPattern));
}
