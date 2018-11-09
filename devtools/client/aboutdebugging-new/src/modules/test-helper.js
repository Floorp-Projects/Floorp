/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const flags = require("devtools/shared/flags");

/**
 * The methods from this module are going to be called from both aboutdebugging
 * BrowserLoader modules (enableMocks) and from tests (setMockedModules,
 * removeMockedModule). We use the main devtools loader to communicate between the
 * different sandboxes involved.
 */
const { Cu } = require("chrome");
const { loader } = Cu.import("resource://devtools/shared/Loader.jsm", {});

const ROOT_PATH = "resource://devtools/client/aboutdebugging-new/src/";

/**
 * Allows to load a mock instead of an about:debugging module.
 * To use it, the module to mock needs to call
 *   require("devtools/client/aboutdebugging-new/src/modules/test-helper").
 *     enableMocks(module, "path/to/module.js");
 *
 * From the tests, the devtools loader should be provided with the mocked module using
 * setMockedModule().
 *
 * At the end of the test, the mocks should be discarded with removeMockedModule().
 *
 * @param {Object} moduleToMock
 *        The `module` global from the module we want to mock, in order to override its
 *        `module.exports`.
 * @param {String} modulePath
 *        Path to the module, relative to `devtools/client/aboutdebugging-new/src/`
 */
function enableMocks(moduleToMock, modulePath) {
  if (flags.testing) {
    const path = ROOT_PATH + modulePath + ".js";
    if (loader.mocks && loader.mocks[path]) {
      moduleToMock.exports = loader.mocks[path];
    }
  }
}
exports.enableMocks = enableMocks;

/**
 * Assign a mock object to the provided module path, relative to
 * `devtools/client/aboutdebugging-new/src/`.
 */
function setMockedModule(mock, modulePath) {
  loader.mocks = loader.mocks || {};
  const mockedModule = {};
  Object.keys(mock).forEach(key => {
    if (typeof mock[key] === "function") {
      // All the mock methods are wrapped again here so that if the test replaces the
      // methods dynamically and call sites used
      // `const { someMethod } = require("./runtime-client-factory")`, someMethod will
      // still internally call the current method defined on the mock, and not the one
      // that was defined at the moment of the import.
      mockedModule[key] = function() {
        return mock[key].apply(mock, arguments);
      };
    } else {
      mockedModule[key] = mock[key];
    }
  });

  const path = ROOT_PATH + modulePath + ".js";
  loader.mocks[path] = mockedModule;
}
exports.setMockedModule = setMockedModule;

/**
 * Remove any mock object defined for the provided module path, relative to
 * `devtools/client/aboutdebugging-new/src/`.
 */
function removeMockedModule(modulePath) {
  const path = ROOT_PATH + modulePath + ".js";
  if (loader.mocks && loader.mocks[path]) {
    delete loader.mocks[path];
  }
}
exports.removeMockedModule = removeMockedModule;
