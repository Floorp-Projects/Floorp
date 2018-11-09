/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../shared/test/shared-head.js */

"use strict";

// This head file contains helpers to create mock versions of the ClientWrapper class
// defined at devtools/client/aboutdebugging-new/src/modules/client-wrapper.js .

const { RUNTIME_PREFERENCE } =
  require("devtools/client/aboutdebugging-new/src/constants");

// Sensible default values for runtime preferences that should be usable in most
// situations
const DEFAULT_PREFERENCES = {
  [RUNTIME_PREFERENCE.CONNECTION_PROMPT]: true,
};

// Creates a simple mock ClientWrapper.
function createClientMock() {
  return {
    // no-op
    addListener: () => {},
    // no-op
    close: () => {},
    // no-op
    connect: () => {},
    // no-op
    getDeviceDescription: () => {},
    // Return default preference value or null if no match.
    getPreference: (prefName) => {
      if (prefName in DEFAULT_PREFERENCES) {
        return DEFAULT_PREFERENCES[prefName];
      }
      return null;
    },
    // Empty array of addons
    listAddons: () => ({ addons: [] }),
    // Empty array of tabs
    listTabs: () => ({ tabs: []}),
    // Empty arrays of workers
    listWorkers: () => ({
      otherWorkers: [],
      serviceWorkers: [],
      sharedWorkers: [],
    }),
    // no-op
    removeListener: () => {},
    // no-op
    setPreference: () => {},
  };
}

// Create a ClientWrapper mock that can be used to replace the this-firefox runtime.
function createThisFirefoxClientMock() {
  const mockThisFirefoxDescription = {
    name: "Firefox",
    channel: "nightly",
    version: "63.0",
  };

  // Create a fake about:debugging tab because our test helper openAboutDebugging
  // waits until about:debugging is displayed in the list of tabs.
  const mockAboutDebuggingTab = {
    outerWindowID: 0,
    url: "about:debugging",
  };

  const mockThisFirefoxClient = createClientMock();
  mockThisFirefoxClient.listTabs = () => ({ tabs: [mockAboutDebuggingTab] });
  mockThisFirefoxClient.getDeviceDescription = () => mockThisFirefoxDescription;

  return mockThisFirefoxClient;
}
