/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Map of mocked modules, keys are absolute URIs for devtools modules such as
// "resource://devtools/path/to/mod.js, values are objects (anything passed to
// setMockedModule technically).
const _mocks = {};

/**
 * Retrieve a mocked module matching the provided uri, eg "resource://path/to/file.js".
 */
function getMockedModule(uri) {
  return _mocks[uri];
}
exports.getMockedModule = getMockedModule;

/**
 * Module paths are transparently provided with or without ".js" when using the loader,
 * normalize the user-provided module paths to always have modules ending with ".js".
 */
function _getUriForModulePath(modulePath) {
  // Assume js modules and add the .js extension if missing.
  if (!modulePath.endsWith(".js")) {
    modulePath = modulePath + ".js";
  }

  // Add resource:// scheme if no scheme is specified.
  if (!modulePath.includes("://")) {
    modulePath = "resource://" + modulePath;
  }

  return modulePath;
}

/**
 * Assign a mock object to the provided module path.
 * @param mock
 *        Plain JavaScript object that will implement the expected API for the mocked
 *        module.
 * @param modulePath
 *        The module path should be the absolute module path, starting with `devtools`:
 *        "devtools/client/some-panel/some-module"
 */
function setMockedModule(mock, modulePath) {
  const uri = _getUriForModulePath(modulePath);
  _mocks[uri] = new Proxy(mock, {
    get(target, key) {
      if (typeof target[key] === "function") {
        // Functions are wrapped to be able to update the methods during the test, even if
        // the methods were imported with destructuring. For instance:
        //   `const { someMethod } = require("./my-module");`
        return function() {
          return target[key].apply(target, arguments);
        };
      }
      return target[key];
    },
  });
}
exports.setMockedModule = setMockedModule;

/**
 * Remove any mock object defined for the provided absolute module path.
 */
function removeMockedModule(modulePath) {
  const uri = _getUriForModulePath(modulePath);
  delete _mocks[uri];
}
exports.removeMockedModule = removeMockedModule;
