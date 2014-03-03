/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Ci, Cu } = require("chrome");
const events = require("../system/events");
const core = require("./core");

const assetsURI = require('../self').data.url();
const { Services } = Cu.import("resource://gre/modules/Services.jsm");

const hideContentStyle = "data:text/css,:root {visibility: hidden !important;}";
const hideSheetUri = Services.io.newURI(hideContentStyle, null, null);

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
    let winUtils = document.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                       .getInterface(Ci.nsIDOMWindowUtils);
    winUtils.removeSheet(hideSheetUri, winUtils.USER_SHEET);
  }
  catch(e) {
    console.exception(e);
  }
}

function onContentWindow(event) {
  let document = event.subject;

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
    let winUtils = document.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                       .getInterface(Ci.nsIDOMWindowUtils);
    winUtils.loadSheet(hideSheetUri, winUtils.USER_SHEET);
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
  if (!enabled) {
    events.on(ON_CONTENT, onContentWindow);
    enabled = true;
  }
}
exports.enable = enable;

function disable() {
  if (enabled) {
    events.off(ON_CONTENT, onContentWindow);
    enabled = false;
  }
}
exports.disable = disable;

require("sdk/system/unload").when(disable);
