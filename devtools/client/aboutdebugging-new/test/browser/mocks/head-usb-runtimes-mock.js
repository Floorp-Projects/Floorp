/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../shared/test/shared-head.js */

"use strict";

/**
 * Setup the loader to return the provided mock object instead of the regular
 * usb-runtimes module.
 *
 * @param {Object}
 *        mock should implement the following methods:
 *        - addUSBRuntimesObserver(listener)
 *        - disableUSBRuntimes()
 *        - enableUSBRuntimes()
 *        - getUSBRuntimes()
 *        - removeUSBRuntimesObserver(listener)
 */
function enableUsbRuntimesMock(mock) {
  const { setMockedModule } = require("devtools/client/aboutdebugging-new/src/modules/test-helper");
  setMockedModule(mock, "modules/usb-runtimes");
}

/**
 * Update the loader to clear the mock entry for the usb-runtimes module.
 */
function disableUsbRuntimesMock() {
  const { removeMockedModule } = require("devtools/client/aboutdebugging-new/src/modules/test-helper");
  removeMockedModule("modules/usb-runtimes");
}

/**
 * Creates a simple mock version for usb-runtimes, implementing all the expected methods
 * with empty placeholders.
 */
function createUsbRuntimesMock() {
  const usbRuntimesMock = {};
  usbRuntimesMock.addUSBRuntimesObserver = function(listener) {
    console.log("MOCKED METHOD addUSBRuntimesObserver");
  };

  usbRuntimesMock.disableUSBRuntimes = function() {
    console.log("MOCKED METHOD disableUSBRuntimes");
  };

  usbRuntimesMock.enableUSBRuntimes = function() {
    console.log("MOCKED METHOD enableUSBRuntimes");
  };

  usbRuntimesMock.getUSBRuntimes = function() {
    console.log("MOCKED METHOD getUSBRuntimes");
  };

  usbRuntimesMock.removeUSBRuntimesObserver = function(listener) {
    console.log("MOCKED METHOD removeUSBRuntimesObserver");
  };

  return usbRuntimesMock;
}

/**
 * The usb-runtimes module allows to observer runtime updates. To simulate this behaviour
 * the easiest is to use an EventEmitter-decorated object that can accept listeners and
 * can emit events from the test.
 *
 * This method will update the addUSBRuntimesObserver method of the provided
 * usbRuntimesMock in order to add listeners to a mockObserver, and returns said observer
 * so that the test can emit "runtime-list-updated" when needed.
 */
function addObserverMock(usbRuntimesMock) {
  const EventEmitter = require("devtools/shared/event-emitter");

  const observerMock = {};
  EventEmitter.decorate(observerMock);
  usbRuntimesMock.addUSBRuntimesObserver = function(listener) {
    console.log("MOCKED METHOD addUSBRuntimesObserver with mock scanner");
    observerMock.on("runtime-list-updated", listener);
  };

  // NOTE FOR REVIEW: Instead of emitting "runtime-list-updated" events in the test,
  // this mock could have a emitObservedEvent method, that would just emit the correct
  // event. This way if the event name changes, everything remains contained in this
  // method.

  return observerMock;
}
