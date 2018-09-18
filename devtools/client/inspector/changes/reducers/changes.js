/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  RESET_CHANGES,
  TRACK_CHANGE,
} = require("../actions/index");

const INITIAL_STATE = {
  /**
   * Diff of changes grouped by stylesheet href, then by selector, then into add/remove
   * objects with CSS property names and values for corresponding changes.
   *
   * Structure:
   *
   *   diff = {
   *    "href": {
   *      "selector": {
   *        add: {
   *          "property": value
   *          ... // more properties
   *        },
   *        remove: {
   *          "property": value
   *          ...
   *        }
   *      },
   *      ... // more selectors
   *    }
   *    ... // more stylesheet hrefs
   *   }
   */
  diff: {},
};

/**
 * Mutate the given diff object with data about a new change.
 *
 * @param  {Object} diff
 *         Diff object from the store.
 * @param  {Object} change
 *         Data about the change: which property was added or removed, on which selector,
 *         in which stylesheet or whether the source is an element's inline style.
 * @return {Object}
 *         Mutated diff object.
 */
function updateDiff(diff = {}, change = {}) {
  // Ensure expected diff structure exists
  diff[change.href] = diff[change.href] || {};
  diff[change.href][change.selector] = diff[change.href][change.selector] || {};
  diff[change.href][change.selector].add = diff[change.href][change.selector].add || {};
  diff[change.href][change.selector].remove = diff[change.href][change.selector].remove
    || {};

  // Reference to the diff data for this stylesheet/selector pair.
  const ref = diff[change.href][change.selector];

  // Track the remove operation only if the property WAS NOT previously introduced by an
  // add operation. This ensures that repeated changes of the same property show up as a
  // single remove operation of the original value.
  if (change.remove && change.remove.property && !ref.add[change.remove.property]) {
    ref.remove[change.remove.property] = change.remove.value;
  }

  if (change.add && change.add.property) {
    ref.add[change.add.property] = change.add.value;
  }

  const propertyName = change.add && change.add.property ||
                   change.remove && change.remove.property;

  // Remove information about operations on the property if they cancel each other out.
  if (ref.add[propertyName] === ref.remove[propertyName]) {
    delete ref.add[propertyName];
    delete ref.remove[propertyName];

    // Remove information about the selector if there are no changes to its declarations.
    if (Object.keys(ref.add).length === 0 && Object.keys(ref.remove).length === 0) {
      delete diff[change.href][change.selector];
    }

    // Remove information about the stylesheet if there are no changes to its rules.
    if (Object.keys(diff[change.href]).length === 0) {
      delete diff[change.href];
    }
  }

  ref.tag = change.tag;

  return diff;
}

const reducers = {

  [TRACK_CHANGE](state, { data }) {
    const defaults = {
      href: "",
      selector: "",
      tag: null,
      add: null,
      remove: null
    };

    data = { ...defaults, ...data };

    // Update the state in-place with data about a style change (no deep clone of state).
    // TODO: redefine state as a shallow object structure after figuring how to track
    // both CSS Declarations and CSS Rules and At-Rules (@media, @keyframes, etc).
    // @See https://bugzilla.mozilla.org/show_bug.cgi?id=1491263
    return Object.assign({}, { diff: updateDiff(state.diff, data) });
  },

  [RESET_CHANGES](state) {
    return INITIAL_STATE;
  },

};

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return state;
  }
  return reducer(state, action);
};
