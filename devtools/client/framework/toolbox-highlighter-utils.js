/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Client-side highlighter shared module.
 * To be used by toolbox panels that need to highlight DOM elements.
 *
 * Highlighting and selecting elements is common enough that it needs to be at
 * toolbox level, accessible by any panel that needs it.
 * That's why the toolbox is the one that initializes the inspector and
 * highlighter. It's also why the API returned by this module needs a reference
 * to the toolbox which should be set once only.
 */

/**
 * Get the highighterUtils instance for a given toolbox.
 * This should be done once only by the toolbox itself and stored there so that
 * panels can get it from there. That's because the API returned has a stateful
 * scope that would be different for another instance returned by this function.
 *
 * @param {Toolbox} toolbox
 * @return {Object} the highlighterUtils public API
 */
exports.getHighlighterUtils = function(toolbox) {
  if (!toolbox) {
    throw new Error("Missing or invalid toolbox passed to getHighlighterUtils");
  }

  // Exported API properties will go here
  const exported = {};

  /**
   * Release this utils, nullifying the references to the toolbox
   */
  exported.release = function() {
    toolbox = null;
  };

  /**
   * Make a function that initializes the inspector before it runs.
   * Since the init of the inspector is asynchronous, the return value will be
   * produced by Task.async and the argument should be a generator
   * @param {Function*} generator A generator function
   * @return {Function} A function
   */
  let isInspectorInitialized = false;
  const requireInspector = generator => {
    return async function(...args) {
      if (!isInspectorInitialized) {
        await toolbox.initInspector();
        isInspectorInitialized = true;
      }
      return generator.apply(null, args);
    };
  };

  // Return the public API
  return exported;
};
