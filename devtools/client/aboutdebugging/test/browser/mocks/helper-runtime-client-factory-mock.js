/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Setup the loader to return the provided mock object instead of the regular
 * runtime-client-factory module.
 *
 * @param {Object}
 *        mock should implement the following methods:
 *        - createClientForRuntime(runtime)
 */
function enableRuntimeClientFactoryMock(mock) {
  const {
    setMockedModule,
  } = require("resource://devtools/shared/loader/browser-loader-mocks.js");
  setMockedModule(
    mock,
    "devtools/client/aboutdebugging/src/modules/runtime-client-factory"
  );

  // When using a mocked client, we should not attempt to check default
  // preferences.
  mockRuntimeDefaultPreferences();
}
/* exported enableRuntimeClientFactoryMock */

const mockRuntimeDefaultPreferences = function () {
  const {
    removeMockedModule,
    setMockedModule,
  } = require("resource://devtools/shared/loader/browser-loader-mocks.js");

  const mock = {
    setDefaultPreferencesIfNeeded: () => {},
    DEFAULT_PREFERENCES: [],
  };
  setMockedModule(
    mock,
    "devtools/client/aboutdebugging/src/modules/runtime-default-preferences"
  );

  registerCleanupFunction(() => {
    removeMockedModule(
      "devtools/client/aboutdebugging/src/modules/runtime-default-preferences"
    );
  });
};

/**
 * Update the loader to clear the mock entry for the runtime-client-factory module.
 */
function disableRuntimeClientFactoryMock() {
  const {
    removeMockedModule,
  } = require("resource://devtools/shared/loader/browser-loader-mocks.js");
  removeMockedModule(
    "devtools/client/aboutdebugging/src/modules/runtime-client-factory"
  );
}
/* exported disableRuntimeClientFactoryMock */

/**
 * Creates a simple mock version for runtime-client-factory, implementing all the expected
 * methods with empty placeholders.
 */
function createRuntimeClientFactoryMock() {
  const RuntimeClientFactoryMock = {};
  RuntimeClientFactoryMock.createClientForRuntime = function (runtime) {
    console.log("MOCKED METHOD createClientForRuntime");
  };

  return RuntimeClientFactoryMock;
}
/* exported createRuntimeClientFactoryMock */
