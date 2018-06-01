/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
"use strict";

/**
 * Execute a keyboard event and check that the state is as expected (focused element, aria
 * attribute etc...).
 *
 * @param {InspectorPanel} inspector
 *        Current instance of the inspector being tested.
 * @param {Object} elms
 *        Map of elements that will be used to retrieve live references to children
 *        elements
 * @param {Element} focused
 *        Element expected to be focused
 * @param {Element} activedescendant
 *        Element expected to be the aria activedescendant of the root node
 */
function testNavigationState(inspector, elms, focused, activedescendant) {
  const doc = inspector.markup.doc;
  const id = activedescendant.getAttribute("id");
  is(doc.activeElement, focused, `Keyboard focus should be set to ${focused}`);
  is(elms.root.elt.getAttribute("aria-activedescendant"), id,
    `Active descendant should be set to ${id}`);
}

/**
 * Execute a keyboard event and check that the state is as expected (focused element, aria
 * attribute etc...).
 *
 * @param {InspectorPanel} inspector
 *        Current instance of the inspector being tested.
 * @param {Object} elms
 *        MarkupContainers/Elements that will be used to retrieve references to other
 *        elements based on objects' paths.
 * @param {Object} testData
 *        - {String} desc: description for better logging.
 *        - {String} key: keyboard event's key.
 *        - {Object} options, optional: event data such as shiftKey, etc.
 *        - {String} focused: path to expected focused element in elms map.
 *        - {String} activedescendant: path to expected aria-activedescendant element in
 *          elms map.
 *        - {String} waitFor, optional: markupview event to wait for if keyboard actions
 *          result in async updates. Also accepts the inspector event "inspector-updated".
 */
async function runAccessibilityNavigationTest(inspector, elms,
  {desc, key, options, focused, activedescendant, waitFor}) {
  info(desc);

  const markup = inspector.markup;
  const doc = markup.doc;
  const win = doc.defaultView;

  let updated;
  if (waitFor) {
    updated = waitFor === "inspector-updated" ?
      inspector.once(waitFor) : markup.once(waitFor);
  } else {
    updated = Promise.resolve();
  }
  EventUtils.synthesizeKey(key, options, win);
  await updated;

  const focusedElement = lookupPath(elms, focused);
  const activeDescendantElement = lookupPath(elms, activedescendant);
  testNavigationState(inspector, elms, focusedElement, activeDescendantElement);
}
