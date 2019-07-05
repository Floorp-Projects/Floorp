/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Setup the loader to return the provided mock object instead of the regular adb module.
 *
 * @param {Object}
 *        mock should implement the following methods:
 *        - registerListener(listener)
 *        - getRuntimes()
 *        - getDevices()
 *        - updateRuntimes()
 *        - unregisterListener(listener)
 */
function enableAdbMock(mock) {
  const {
    setMockedModule,
  } = require("devtools/client/shared/browser-loader-mocks");
  setMockedModule(mock, "devtools/shared/adb/adb");
}
/* exported enableAdbMock */

/**
 * Update the loader to clear the mock entry for the adb module.
 */
function disableAdbMock() {
  const {
    removeMockedModule,
  } = require("devtools/client/shared/browser-loader-mocks");
  removeMockedModule("devtools/shared/adb/adb");
}
/* exported disableAdbMock */

/**
 * Creates a simple mock version for adb, implementing all the expected methods
 * with empty placeholders.
 */
function createAdbMock() {
  const adbMock = {};
  adbMock.registerListener = function(listener) {
    console.log("MOCKED METHOD registerListener");
  };

  adbMock.getRuntimes = function() {
    console.log("MOCKED METHOD getRuntimes");
  };

  adbMock.getDevices = function() {
    console.log("MOCKED METHOD getDevices");
  };

  adbMock.updateRuntimes = function() {
    console.log("MOCKED METHOD updateRuntimes");
  };

  adbMock.unregisterListener = function(listener) {
    console.log("MOCKED METHOD unregisterListener");
  };

  adbMock.once = function() {
    console.log("MOCKED METHOD once");
  };

  adbMock.isProcessStarted = function() {
    console.log("MOCKED METHOD isProcessStarted");
  };

  return { adb: adbMock };
}
/* exported createAdbMock */

/**
 * The adb module allows to observe runtime updates. To simulate this behaviour
 * the easiest is to use an EventEmitter-decorated object that can accept listeners and
 * can emit events from the test.
 *
 * This method will update the registerListener method of the provided
 * usbRuntimesMock in order to add listeners to a mockObserver, and returns said observer
 * so that the test can emit "runtime-list-updated" when needed.
 */
function addObserverMock(adbMock) {
  const EventEmitter = require("devtools/shared/event-emitter");

  const observerMock = {};
  EventEmitter.decorate(observerMock);
  adbMock.registerListener = function(listener) {
    console.log("MOCKED METHOD registerListener with mock scanner");
    observerMock.on("runtime-list-updated", listener);
  };

  // NOTE FOR REVIEW: Instead of emitting "runtime-list-updated" events in the test,
  // this mock could have a emitObservedEvent method, that would just emit the correct
  // event. This way if the event name changes, everything remains contained in this
  // method.

  return observerMock;
}
/* exported addObserverMock */

function createAdbProcessMock() {
  const EventEmitter = require("devtools/shared/event-emitter");

  const mock = {};
  EventEmitter.decorate(mock);

  mock.ready = false;

  mock.start = async () => {
    console.log("MOCKED METHOD start");
    mock.ready = true;
    mock.emit("adb-ready");
  };

  return { adbProcess: mock };
}
/* exported createAdbProcessMock */

function enableAdbProcessMock(mock) {
  const {
    setMockedModule,
  } = require("devtools/client/shared/browser-loader-mocks");
  setMockedModule(mock, "devtools/shared/adb/adb-process");
}
/* exported enableAdbProcessMock */

function disableAdbProcessMock() {
  const {
    removeMockedModule,
  } = require("devtools/client/shared/browser-loader-mocks");
  removeMockedModule("devtools/shared/adb/adb-process");
}
/* exported disableAdbProcessMock */
