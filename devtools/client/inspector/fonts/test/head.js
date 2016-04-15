 /* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

Services.prefs.setBoolPref("devtools.fontinspector.enabled", true);
Services.prefs.setCharPref("devtools.inspector.activeSidebar", "fontinspector");
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.fontinspector.enabled");
});

/**
 * The font-inspector doesn't participate in the inspector's update mechanism
 * (i.e. it doesn't call inspector.updating() when updating), so simply calling
 * the default selectNode isn't enough to guaranty that the panel has finished
 * updating. We also need to wait for the fontinspector-updated event.
 */
var _selectNode = selectNode;
selectNode = function* (node, inspector, reason) {
  let onUpdated = inspector.once("fontinspector-updated");
  yield _selectNode(node, inspector, reason);
  yield onUpdated;
};

/**
 * Adds a new tab with the given URL, opens the inspector and selects the
 * font-inspector tab.
 * @return {Promise} resolves to a {toolbox, inspector, view} object
 */
var openFontInspectorForURL = Task.async(function*(url) {
  yield addTab(url);
  let {toolbox, inspector} = yield openInspector();

  // Call selectNode again here to force a fontinspector update since we don't
  // know if the fontinspector-updated event has been sent while the inspector
  // was being opened or not.
  yield selectNode("body", inspector);

  return {
    toolbox,
    inspector,
    view: inspector.fontInspector
  };
});

/**
 * Clears the preview input field, types new text into it and waits for the
 * preview images to be updated.
 *
 * @param {FontInspector} view - The FontInspector instance.
 * @param {String} text - The text to preview.
 */
function* updatePreviewText(view, text) {
  info(`Changing the preview text to '${text}'`);

  let doc = view.chromeDoc;
  let input = doc.getElementById("font-preview-text-input");
  let update = view.inspector.once("fontinspector-updated");

  info("Focusing the input field.");
  input.focus();

  is(doc.activeElement, input, "The input was focused.");

  info("Blanking the input field.");
  for (let i = input.value.length; i >= 0; i--) {
    EventUtils.sendKey("BACK_SPACE", doc.defaultView);
  }

  is(input.value, "", "The input is now blank.");

  info("Typing the specified text to the input field.");
  EventUtils.sendString(text, doc.defaultView);
  is(input.value, text, "The input now contains the correct text.");

  info("Waiting for the font-inspector to update.");
  yield update;
}
