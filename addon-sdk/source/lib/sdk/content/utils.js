/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

let assetsURI = require("../self").data.url();
let isArray = Array.isArray;

function isAddonContent({ contentURL }) {
  return typeof(contentURL) === "string" && contentURL.indexOf(assetsURI) === 0;
}
exports.isAddonContent = isAddonContent;

function hasContentScript({ contentScript, contentScriptFile }) {
  return (isArray(contentScript) ? contentScript.length > 0 :
         !!contentScript) ||
         (isArray(contentScriptFile) ? contentScriptFile.length > 0 :
         !!contentScriptFile);
}
exports.hasContentScript = hasContentScript;

function requiresAddonGlobal(model) {
  return isAddonContent(model) && !hasContentScript(model);
}
exports.requiresAddonGlobal = requiresAddonGlobal;

function getAttachEventType(model) {
  if (!model) return null;
  let when = model.contentScriptWhen;
  return requiresAddonGlobal(model) ? "document-element-inserted" :
         when === "start" ? "document-element-inserted" :
         when === "end" ? "load" :
         when === "ready" ? "DOMContentLoaded" :
         null;
}
exports.getAttachEventType = getAttachEventType;

