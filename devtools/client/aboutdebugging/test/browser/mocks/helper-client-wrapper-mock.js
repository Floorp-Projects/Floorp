/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict"; // defined in head.js

/* global CHROME_URL_ROOT */

// This head file contains helpers to create mock versions of the ClientWrapper class
// defined at devtools/client/aboutdebugging/src/modules/client-wrapper.js .

const {
  RUNTIME_PREFERENCE,
} = require("devtools/client/aboutdebugging/src/constants");

// Sensible default values for runtime preferences that should be usable in most
// situations
const DEFAULT_PREFERENCES = {
  [RUNTIME_PREFERENCE.CONNECTION_PROMPT]: true,
  [RUNTIME_PREFERENCE.PERMANENT_PRIVATE_BROWSING]: false,
  [RUNTIME_PREFERENCE.SERVICE_WORKERS_ENABLED]: true,
};

// Creates a simple mock ClientWrapper.
function createClientMock() {
  const EventEmitter = require("devtools/shared/event-emitter");
  const eventEmitter = {};
  EventEmitter.decorate(eventEmitter);

  return {
    // add a reference to the internal event emitter so that consumers can fire client
    // events.
    _eventEmitter: eventEmitter,
    _preferences: {},
    contentProcessFronts: [],
    serviceWorkerRegistrationFronts: [],
    once: (evt, listener) => {
      eventEmitter.once(evt, listener);
    },
    on: (evt, listener) => {
      eventEmitter.on(evt, listener);
    },
    off: (evt, listener) => {
      eventEmitter.off(evt, listener);
    },
    client: {
      once: (evt, listener) => {
        eventEmitter.once(evt, listener);
      },
      on: (evt, listener) => {
        eventEmitter.on(evt, listener);
      },
      off: (evt, listener) => {
        eventEmitter.off(evt, listener);
      },
    },
    // no-op
    close: () => {},
    // client is not closed
    isClosed: () => false,
    // no-op
    connect: () => {},
    // no-op
    getDeviceDescription: () => {},
    // Return default preference value or null if no match.
    getPreference: function(prefName) {
      if (prefName in this._preferences) {
        return this._preferences[prefName];
      }
      if (prefName in DEFAULT_PREFERENCES) {
        return DEFAULT_PREFERENCES[prefName];
      }
      return null;
    },
    // Empty array of addons
    listAddons: () => [],
    // Empty array of processes
    listProcesses: () => [],
    // Empty array of tabs
    listTabs: () => [],
    // Empty arrays of workers
    listWorkers: () => ({
      otherWorkers: [],
      serviceWorkers: [],
      sharedWorkers: [],
    }),
    // no-op
    getMainProcess: () => {},
    // no-op
    getFront: () => {},
    // stores the preference locally (doesn't update about:config)
    setPreference: function(prefName, value) {
      this._preferences[prefName] = value;
    },
    getPerformancePanelUrl: () => CHROME_URL_ROOT + "empty.html",
    loadPerformanceProfiler: () => {},
    // Valid compatibility report
    checkVersionCompatibility: () => {
      const {
        COMPATIBILITY_STATUS,
      } = require("devtools/client/shared/remote-debugging/version-checker");
      return { status: COMPATIBILITY_STATUS.COMPATIBLE };
    },
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
    getFavicon: () => {},
    outerWindowID: 0,
    traits: {
      getFavicon: true,
    },
    url: "about:debugging",
  };

  const mockThisFirefoxClient = createClientMock();
  mockThisFirefoxClient.listTabs = () => [mockAboutDebuggingTab];
  mockThisFirefoxClient.getDeviceDescription = () => mockThisFirefoxDescription;

  return mockThisFirefoxClient;
}
/* exported createThisFirefoxClientMock */
