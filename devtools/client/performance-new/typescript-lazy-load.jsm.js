/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * TypeScript can't understand the lazyRequireGetter mechanism, due to how it defines
 * properties as a getter. This function, instead provides lazy loading in a
 * TypeScript-friendly manner. It applies the lazy load memoization to each property
 * of the provided object.
 *
 * Example usage:
 *
 * const lazy = createLazyLoaders({
 *   moduleA: () => require("module/a"),
 *   moduleB: () => require("module/b"),
 * });
 *
 * Later:
 *
 * const moduleA = lazy.moduleA();
 * const { objectInModuleB } = lazy.moduleB();
 *
 * @template T
 * @param {T} definition - An object where each property has a function that loads a module.
 * @returns {T} - The load memoized version of T.
 */
function createLazyLoaders(definition) {
  /** @type {any} */
  const result = {};
  for (const [key, callback] of Object.entries(definition)) {
    /** @type {any} */
    let cache;
    result[key] = () => {
      if (cache === undefined) {
        cache = callback();
      }
      return cache;
    };
  }
  return result;
}

// Provide an exports object for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).module = {};

module.exports = {
  createLazyLoaders,
};

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(module.exports);
