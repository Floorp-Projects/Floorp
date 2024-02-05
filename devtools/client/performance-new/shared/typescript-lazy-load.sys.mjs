/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

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
 * @template {{ [key: string]: any }} T
 * @param {T} definition - An object where each property has a function that loads a module.
 * @returns {T} - The load memoized version of T.
 */
export function createLazyLoaders(definition) {
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
