/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

Services.prefs.setCharPref("devtools.inspector.activeSidebar", "fontinspector");
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
  Services.prefs.clearUserPref("devtools.inspector.selectedSidebar");
});

var nodeConstants = require("resource://devtools/shared/dom-node-constants.js");

/**
 * The font-inspector doesn't participate in the inspector's update mechanism
 * (i.e. it doesn't call inspector.updating() when updating), so simply calling
 * the default selectNode isn't enough to guaranty that the panel has finished
 * updating. We also need to wait for the fontinspector-updated event.
 *
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox.
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not to highlight the
 *        node upon selection.
 */
var _selectNode = selectNode;
selectNode = async function (node, inspector, reason) {
  // Ensure node is a NodeFront and not a selector (which is also accepted as
  // an argument to selectNode).
  node = await getNodeFront(node, inspector);

  // The FontInspector will fallback to the parent node when a text node is
  // selected.
  const isTextNode = node.nodeType == nodeConstants.TEXT_NODE;
  const expectedNode = isTextNode ? node.parentNode() : node;

  const onEditorUpdated = inspector.once("fonteditor-updated");
  const onFontInspectorUpdated = new Promise(resolve => {
    inspector.on("fontinspector-updated", function onUpdated(eventNode) {
      if (eventNode === expectedNode) {
        inspector.off("fontinspector-updated", onUpdated);
        resolve();
      }
    });
  });
  await _selectNode(node, inspector, reason);

  // Wait for both the font inspector and font editor before proceeding.
  await Promise.all([onFontInspectorUpdated, onEditorUpdated]);
};

/**
 * Adds a new tab with the given URL, opens the inspector and selects the
 * font-inspector tab.
 * @return {Promise} resolves to a {tab, toolbox, inspector, view} object
 */
var openFontInspectorForURL = async function (url) {
  const tab = await addTab(url);
  const { toolbox, inspector } = await openInspector();

  // Call selectNode again here to force a fontinspector update since we don't
  // know if the fontinspector-updated event has been sent while the inspector
  // was being opened or not.
  await selectNode("body", inspector);

  return {
    tab,
    toolbox,
    inspector,
    view: inspector.getPanel("fontinspector"),
  };
};

/**
 * Focus the preview input, clear it, type new text into it and wait for the
 * preview images to be updated.
 *
 * @param {FontInspector} view - The FontInspector instance.
 * @param {String} text - The text to preview.
 */
async function updatePreviewText(view, text) {
  info(`Changing the preview text to '${text}'`);

  const doc = view.document;
  const input = doc.querySelector("#font-preview-input-container input");
  input.focus();

  info("Blanking the input field.");
  while (input.value.length) {
    const update = view.inspector.once("fontinspector-updated");
    EventUtils.sendKey("BACK_SPACE", doc.defaultView);
    await update;
  }

  if (text) {
    info(`Typing "${text}" into the input field.`);
    const update = view.inspector.once("fontinspector-updated");
    EventUtils.sendString(text, doc.defaultView);
    await update;
  }

  is(input.value, text, `The input now contains "${text}".`);
}

/**
 *  Get all of the <li> elements for the fonts used on the currently selected element.
 *
 *  NOTE: This method is used by tests which check the old Font Inspector. It, along with
 *  the tests should be removed once the Font Editor reaches Firefox Stable.
 *  @see https://bugzilla.mozilla.org/show_bug.cgi?id=1485324
 *
 * @param  {Document} viewDoc
 * @return {NodeList}
 */
function getUsedFontsEls_obsolete(viewDoc) {
  return viewDoc.querySelectorAll("#font-editor .fonts-list li");
}

/**
 * Get all of the elements with names of fonts used on the currently selected element.
 *
 * @param  {Document} viewDoc
 * @return {NodeList}
 */
function getUsedFontsEls(viewDoc) {
  return viewDoc.querySelectorAll(
    "#font-editor .font-control-used-fonts .font-name"
  );
}

/**
 * Get all of the elements with groups of fonts used on the currently selected element.
 *
 * @param  {Document} viewDoc
 * @return {NodeList}
 */
function getUsedFontGroupsEls(viewDoc) {
  return viewDoc.querySelectorAll(
    "#font-editor .font-control-used-fonts .font-group"
  );
}

/**
 * Get the DOM element for the accordion widget that contains the fonts used elsewhere in
 * the document.
 *
 * @param  {Document} viewDoc
 * @return {DOMNode}
 */
function getFontsAccordion(viewDoc) {
  return viewDoc.querySelector("#font-container .accordion");
}

/**
 * Expand a given accordion widget.
 *
 * @param  {DOMNode} accordion
 */
async function expandAccordion(accordion) {
  const isExpanded = () => accordion.querySelector(".fonts-list");
  if (isExpanded()) {
    return;
  }

  const onExpanded = BrowserTestUtils.waitForCondition(
    isExpanded,
    "Waiting for other fonts section"
  );
  accordion.querySelector(".theme-twisty").click();
  await onExpanded;
}

/**
 * Expand the fonts accordion.
 *
 * @param  {Document} viewDoc
 */
async function expandFontsAccordion(viewDoc) {
  info("Expanding the other fonts section");
  await expandAccordion(getFontsAccordion(viewDoc));
}

/**
 * Get all of the <li> elements for the fonts used elsewhere in the document.
 *
 * @param  {Document} viewDoc
 * @return {NodeList}
 */
function getAllFontsEls(viewDoc) {
  return getFontsAccordion(viewDoc).querySelectorAll(".fonts-list > li");
}

/**
 * Given a font element, return its name.
 *
 * @param  {DOMNode} fontEl
 *         The font element.
 * @return {String}
 *         The name of the font as shown in the UI.
 */
function getName(fontEl) {
  return fontEl.querySelector(".font-name").textContent;
}

/**
 * Given a font element, return the font's URL.
 *
 * @param  {DOMNode} fontEl
 *         The font element.
 * @return {String}
 *         The URL where the font was loaded from as shown in the UI.
 */
function getURL(fontEl) {
  return fontEl.querySelector(".font-origin").textContent;
}

/**
 * Given a font element, return its family name.
 *
 * @param  {DOMNode} fontEl
 *         The font element.
 * @return {String}
 *         The name of the font family as shown in the UI.
 */
function getFamilyName(fontEl) {
  return fontEl.querySelector(".font-family-name").textContent;
}

/**
 * Get the value and unit of a CSS font property or font axis from the font editor.
 *
 * @param  {Document} viewDoc
 *         Host document of the font inspector panel.
 * @param  {String} name
 *         Font property name or axis tag
 * @return {Object}
 *         Object with the value and unit of the given font property or axis tag
 *         from the corresponding input fron the font editor.
 *         @Example:
 *         {
 *          value: {String|null}
 *          unit: {String|null}
 *         }
 */
function getPropertyValue(viewDoc, name) {
  const selector = `#font-editor .font-value-input[name=${name}]`;
  return {
    // Ensure value input exists before querying its value
    value:
      viewDoc.querySelector(selector) &&
      parseFloat(viewDoc.querySelector(selector).value),
    // Ensure unit dropdown exists before querying its value
    unit:
      viewDoc.querySelector(selector + ` ~ .font-value-select`) &&
      viewDoc.querySelector(selector + ` ~ .font-value-select`).value,
  };
}

/**
 * Given a font element, check whether its font source is remote.
 *
 * @param  {DOMNode} fontEl
 *         The font element.
 * @return {Boolean}
 */
function isRemote(fontEl) {
  return fontEl.querySelector(".font-origin").classList.contains("remote");
}
