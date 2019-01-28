 /* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
/* import-globals-from ../../../inspector/rules/test/head.js */
/* import-globals-from ../../../inspector/test/shared-head.js */
/* import-globals-from ../../../shared/test/shared-redux-head.js */
"use strict";

// Load the Rule view's test/head.js to make use of its helpers.
// It loads inspector/test/head.js which itself loads inspector/test/shared-head.js
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/rules/test/head.js",
  this);

// Load the shared Redux helpers.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

// Ensure the Changes panel is enabled before running the tests.
Services.prefs.setBoolPref("devtools.inspector.changes.enabled", true);
// Ensure the three-pane mode is enabled before running the tests.
Services.prefs.setBoolPref("devtools.inspector.three-pane-enabled", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.changes.enabled");
  Services.prefs.clearUserPref("devtools.inspector.three-pane-enabled");
});

/**
 * Get an array of objects with property/value pairs of the CSS declarations rendered
 * in the Changes panel.
 *
 * @param  {Document} panelDoc
 *         Host document of the Changes panel.
 * @param  {String} selector
 *         Optional selector to filter rendered declaration DOM elements.
 *         One of ".diff-remove" or ".diff-add".
 *         If omitted, all declarations will be returned.
 * @param  {DOMNode} containerNode
 *         Optional element to restrict results to declaration DOM elements which are
 *         descendants of this container node.
 *         If omitted, all declarations will be returned
 * @return {Array}
 */
function getDeclarations(panelDoc, selector = "", containerNode = null) {
  const els = panelDoc.querySelectorAll(`#sidebar-panel-changes .declaration${selector}`);

  return [...els]
    .filter(el => {
      return containerNode ? containerNode.contains(el) : true;
    })
    .map(el => {
      return {
        property: el.querySelector(".declaration-name").textContent,
        value: el.querySelector(".declaration-value").textContent,
      };
    });
}

function getAddedDeclarations(panelDoc, containerNode) {
  return getDeclarations(panelDoc, ".diff-add", containerNode);
}

function getRemovedDeclarations(panelDoc, containerNode) {
  return getDeclarations(panelDoc, ".diff-remove", containerNode);
}
