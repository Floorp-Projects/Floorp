/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported addPerfTest, MockPerfFront */
/* globals URL_ROOT */

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);
var { require } = BrowserLoader({
  baseURI: "resource://devtools/client/performance-new/",
  window,
});

const EventEmitter = require("devtools/shared/event-emitter");
const { perfDescription } = require("devtools/shared/specs/perf");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

const EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;
SimpleTest.registerCleanupFunction(function() {
  if (
    DevToolsUtils.assertionFailureCount !== EXPECTED_DTU_ASSERT_FAILURE_COUNT
  ) {
    ok(
      false,
      "Should have had the expected number of DevToolsUtils.assert() failures." +
        "Expected " +
        EXPECTED_DTU_ASSERT_FAILURE_COUNT +
        ", got " +
        DevToolsUtils.assertionFailureCount
    );
  }
});

/**
 * Handle test setup and teardown while catching errors.
 */
function addPerfTest(asyncTest) {
  window.onload = async () => {
    try {
      await asyncTest();
    } catch (e) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(e));
    } finally {
      SimpleTest.finish();
    }
  };
}

/**
 * The Gecko Profiler is a rather heavy-handed component that uses a lot of resources.
 * In order to get around that, and have quick component tests we provide a mock of
 * the performance front. It also has a method called _flushAsyncQueue() that will
 * flush any queued async calls to deterministically run our tests.
 */
class MockPerfFront extends EventEmitter {
  constructor() {
    super();
    this._isActive = false;
    this._asyncQueue = [];
    this._startProfilerCalls = [];

    // Tests can update these two values directly as needed.
    this.mockIsSupported = true;
    this.mockIsLocked = false;

    // Wrap all async methods in a flushable queue, so that tests can control
    // when the responses come back.
    this.isActive = this._wrapInAsyncQueue(this.isActive);
    this.startProfiler = this._wrapInAsyncQueue(this.startProfiler);
    this.stopProfilerAndDiscardProfile = this._wrapInAsyncQueue(
      this.stopProfilerAndDiscardProfile
    );
    this.getProfileAndStopProfiler = this._wrapInAsyncQueue(
      this.getProfileAndStopProfiler
    );
  }

  /**
   * Provide a flushable queue mechanism for all async work. The work piles up
   * and then is evaluated at once when _flushPendingQueue is called.
   */
  _wrapInAsyncQueue(fn) {
    if (typeof fn !== "function") {
      throw new Error("_wrapInAsyncQueue requires a function");
    }
    return (...args) => {
      return new Promise(resolve => {
        this._asyncQueue.push(() => {
          resolve(fn.apply(this, args));
        });
      });
    };
  }

  /**
   * This method is in the mock only, and it serves to flush out any pending async calls
   * so that tests can properly wait on asynchronous behavior.
   */
  _flushAsyncQueue() {
    const pending = this._asyncQueue;
    this._asyncQueue = [];
    pending.forEach(fn => fn());
    // Ensure this is async.
    return new Promise(resolve => setTimeout(resolve, 0));
  }

  startProfiler(settings) {
    this._startProfilerCalls.push(settings);
    this._isActive = true;
    this.emit("profiler-started");
  }

  getProfileAndStopProfiler() {
    this._isActive = false;
    this.emit("profiler-stopped");
    // Return a fake profile.
    return { meta: {}, libs: [], threads: [], processes: [] };
  }

  async getSymbolTable() {
    throw new Error("unimplemented");
  }

  stopProfilerAndDiscardProfile() {
    this._isActive = false;
    this.emit("profiler-stopped");
  }

  isActive() {
    return this._isActive;
  }

  isSupportedPlatform() {
    return this.mockIsSupported;
  }

  isLockedForPrivateBrowsing() {
    return this.mockIsLocked;
  }
}

// Do a quick validation to make sure that our Mock has the same methods as a spec.
const mockKeys = Object.getOwnPropertyNames(MockPerfFront.prototype);
Object.getOwnPropertyNames(perfDescription.methods).forEach(methodName => {
  if (!mockKeys.includes(methodName)) {
    throw new Error(
      `The MockPerfFront is missing the method "${methodName}" from the ` +
        "actor's spec. It should be added to the mock."
    );
  }
});

/**
 * Set a React-friendly input value. Doing this the normal way doesn't work.
 *
 * See: https://github.com/facebook/react/issues/10135#issuecomment-314441175
 */
function setReactFriendlyInputValue(element, value) {
  const valueSetter = Object.getOwnPropertyDescriptor(element, "value").set;
  const prototype = Object.getPrototypeOf(element);
  const prototypeValueSetter = Object.getOwnPropertyDescriptor(
    prototype,
    "value"
  ).set;

  if (valueSetter && valueSetter !== prototypeValueSetter) {
    prototypeValueSetter.call(element, value);
  } else {
    valueSetter.call(element, value);
  }
  element.dispatchEvent(new Event("input", { bubbles: true }));
}

/**
 * This is a helper function to correctly mount the Perf component, and provide
 * mocks where needed.
 */
function createPerfComponent() {
  const Perf = require("devtools/client/performance-new/components/Perf");
  const React = require("devtools/client/shared/vendor/react");
  const ReactDOM = require("devtools/client/shared/vendor/react-dom");
  const ReactRedux = require("devtools/client/shared/vendor/react-redux");
  const createStore = require("devtools/client/shared/redux/create-store");
  const reducers = require("devtools/client/performance-new/store/reducers");
  const actions = require("devtools/client/performance-new/store/actions");
  const selectors = require("devtools/client/performance-new/store/selectors");

  const perfFrontMock = new MockPerfFront();
  const store = createStore(reducers);
  const container = document.querySelector("#container");
  const receiveProfileCalls = [];
  const recordingPreferencesCalls = [];

  function receiveProfileMock(profile, getSymbolTableCallback) {
    receiveProfileCalls.push(profile);
  }

  function recordingPreferencesMock(settings) {
    recordingPreferencesCalls.push(settings);
  }

  function mountComponent() {
    store.dispatch(
      actions.initializeStore({
        perfFront: perfFrontMock,
        receiveProfile: receiveProfileMock,
        recordingSettingsFromPreferences: selectors.getRecordingSettings(
          store.getState()
        ),
        setRecordingPreferences: recordingPreferencesMock,
      })
    );

    return ReactDOM.render(
      React.createElement(
        ReactRedux.Provider,
        { store },
        React.createElement(Perf)
      ),
      container
    );
  }

  /**
   * The perf front is initially queried for the status of the profiler, flush
   * those requests to have a fully initializated component.
   */
  async function mountAndInitializeComponent() {
    mountComponent();
    await perfFrontMock._flushAsyncQueue();
  }

  // Provide a list of common values that may be needed during testing.
  return {
    receiveProfileCalls,
    recordingPreferencesCalls,
    perfFrontMock,
    mountComponent,
    mountAndInitializeComponent,
    selectors,
    store,
    container,
    getState: store.getState,
    dispatch: store.dispatch,
    // Provide a common shortcut for this selector.
    getRecordingState: () => selectors.getRecordingState(store.getState()),
  };
}
