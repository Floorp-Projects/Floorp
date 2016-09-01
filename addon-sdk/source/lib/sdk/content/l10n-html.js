/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Ci, Cc, Cu } = require("chrome");
const core = require("../l10n/core");
const { loadSheet, removeSheet } = require("../stylesheet/utils");
const { process, frames } = require("../remote/child");
var observerService = Cc["@mozilla.org/observer-service;1"]
                      .getService(Ci.nsIObserverService);
const { ShimWaiver } = Cu.import("resource://gre/modules/ShimWaiver.jsm");
const addObserver = ShimWaiver.getProperty(observerService, "addObserver");
const removeObserver = ShimWaiver.getProperty(observerService, "removeObserver");

const assetsURI = require('../self').data.url();

const hideSheetUri = "data:text/css,:root {visibility: hidden !important;}";

function translateElementAttributes(element) {
  // Translateable attributes
  const attrList = ['title', 'accesskey', 'alt', 'label', 'placeholder'];
  const ariaAttrMap = {
          'ariaLabel': 'aria-label',
          'ariaValueText': 'aria-valuetext',
          'ariaMozHint': 'aria-moz-hint'
        };
  const attrSeparator = '.';
  
  // Try to translate each of the attributes
  for (let attribute of attrList) {
    const data = core.get(element.dataset.l10nId + attrSeparator + attribute);
    if (data)
      element.setAttribute(attribute, data);
  }
  
  // Look for the aria attribute translations that match fxOS's aliases
  for (let attrAlias in ariaAttrMap) {
    const data = core.get(element.dataset.l10nId + attrSeparator + attrAlias);
    if (data)
      element.setAttribute(ariaAttrMap[attrAlias], data);
  }
}

// Taken from Gaia:
// https://github.com/andreasgal/gaia/blob/04fde2640a7f40314643016a5a6c98bf3755f5fd/webapi.js#L1470
function translateElement(element) {
  element = element || document;

  // check all translatable children (= w/ a `data-l10n-id' attribute)
  var children = element.querySelectorAll('*[data-l10n-id]');
  var elementCount = children.length;
  for (var i = 0; i < elementCount; i++) {
    var child = children[i];

    // translate the child
    var key = child.dataset.l10nId;
    var data = core.get(key);
    if (data)
      child.textContent = data;

    translateElementAttributes(child);
  }
}
exports.translateElement = translateElement;

function onDocumentReady2Translate(event) {
  let document = event.target;
  document.removeEventListener("DOMContentLoaded", onDocumentReady2Translate,
                               false);

  translateElement(document);

  try {
    // Finally display document when we finished replacing all text content
    if (document.defaultView)
      removeSheet(document.defaultView, hideSheetUri, 'user');
  }
  catch(e) {
    console.exception(e);
  }
}

function onContentWindow(document) {
  // Accept only HTML documents
  if (!(document instanceof Ci.nsIDOMHTMLDocument))
    return;

  // Bug 769483: data:URI documents instanciated with nsIDOMParser
  // have a null `location` attribute at this time
  if (!document.location)
    return;

  // Accept only document from this addon
  if (document.location.href.indexOf(assetsURI) !== 0)
    return;

  try {
    // First hide content of the document in order to have content blinking
    // between untranslated and translated states
    loadSheet(document.defaultView, hideSheetUri, 'user');
  }
  catch(e) {
    console.exception(e);
  }
  // Wait for DOM tree to be built before applying localization
  document.addEventListener("DOMContentLoaded", onDocumentReady2Translate,
                            false);
}

// Listen to creation of content documents in order to translate them as soon
// as possible in their loading process
const ON_CONTENT = "document-element-inserted";
let enabled = false;
function enable() {
  if (enabled)
    return;
  addObserver(onContentWindow, ON_CONTENT, false);
  enabled = true;
}
process.port.on("sdk/l10n/html/enable", enable);

function disable() {
  if (!enabled)
    return;
  removeObserver(onContentWindow, ON_CONTENT);
  enabled = false;
}
process.port.on("sdk/l10n/html/disable", disable);
