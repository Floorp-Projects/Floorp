/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

add_task(function testAbortSingleListener() {
  // Test a simple case with AbortController
  info("Create an EventEmitter");
  const emitter = new EventEmitter();
  const abortController = new AbortController();
  const { signal } = abortController;

  info("Setup an event listener on test-event, controlled by an AbortSignal");
  let eventsReceived = 0;
  emitter.on("test-event", () => eventsReceived++, { signal });

  info("Emit test-event");
  emitter.emit("test-event");
  equal(eventsReceived, 1, "We received one event, as expected");

  info("Abort the AbortController…");
  abortController.abort();
  info("… and emit test-event again");
  emitter.emit("test-event");
  equal(eventsReceived, 1, "We didn't receive new event after aborting");
});

add_task(function testAbortSingleListenerOnce() {
  // Test a simple case with AbortController and once
  info("Create an EventEmitter");
  const emitter = new EventEmitter();
  const abortController = new AbortController();
  const { signal } = abortController;

  info("Setup an event listener on test-event, controlled by an AbortSignal");
  let eventReceived = false;
  emitter.once(
    "test-event",
    () => {
      eventReceived = true;
    },
    { signal }
  );

  info("Abort the AbortController…");
  abortController.abort();
  info("… and emit test-event");
  emitter.emit("test-event");
  equal(eventReceived, false, "We didn't receive the event after aborting");
});

add_task(function testAbortMultipleListener() {
  // Test aborting multiple event listeners with one call to abort
  info("Create an EventEmitter");
  const emitter = new EventEmitter();
  const abortController = new AbortController();
  const { signal } = abortController;

  info("Setup 3 event listeners controlled by an AbortSignal");
  let eventsReceived = 0;
  emitter.on("test-event", () => eventsReceived++, { signal });
  emitter.on("test-event", () => eventsReceived++, { signal });
  emitter.on("other-test-event", () => eventsReceived++, { signal });

  info("Emit test-event and other-test-event");
  emitter.emit("test-event");
  emitter.emit("other-test-event");
  equal(eventsReceived, 3, "We received 3 events, as expected");

  info("Abort the AbortController…");
  abortController.abort();
  info("… and emit events again");
  emitter.emit("test-event");
  emitter.emit("other-test-event");
  equal(eventsReceived, 3, "We didn't receive new event after aborting");
});

add_task(function testAbortMultipleEmitter() {
  // Test aborting multiple event listeners on different emitters with one call to abort
  info("Create 2 EventEmitter");
  const emitter1 = new EventEmitter();
  const emitter2 = new EventEmitter();
  const abortController = new AbortController();
  const { signal } = abortController;

  info("Setup 2 event listeners on test-event, controlled by an AbortSignal");
  let eventsReceived = 0;
  emitter1.on("test-event", () => eventsReceived++, { signal });
  emitter2.on("other-test-event", () => eventsReceived++, { signal });

  info("Emit test-event and other-test-event");
  emitter1.emit("test-event");
  emitter2.emit("other-test-event");
  equal(eventsReceived, 2, "We received 2 events, as expected");

  info("Abort the AbortController…");
  abortController.abort();
  info("… and emit events again");
  emitter1.emit("test-event");
  emitter2.emit("other-test-event");
  equal(eventsReceived, 2, "We didn't receive new event after aborting");
});

add_task(function testAbortBeforeEmitting() {
  // Check that aborting before emitting does unregister the event listener
  info("Create an EventEmitter");
  const emitter = new EventEmitter();
  const abortController = new AbortController();
  const { signal } = abortController;

  info("Setup an event listener on test-event, controlled by an AbortSignal");
  let eventsReceived = 0;
  emitter.on("test-event", () => eventsReceived++, { signal });

  info("Abort the AbortController…");
  abortController.abort();

  info("… and emit test-event");
  emitter.emit("test-event");
  equal(eventsReceived, 0, "We didn't receive any event");
});

add_task(function testAbortBeforeSettingListener() {
  // Check that aborting before creating the event listener won't register it
  info("Create an EventEmitter");
  const emitter = new EventEmitter();

  info("Create an AbortController and abort it immediately");
  const abortController = new AbortController();
  const { signal } = abortController;
  abortController.abort();

  info(
    "Setup an event listener on test-event, controlled by the aborted AbortSignal"
  );
  let eventsReceived = 0;
  const off = emitter.on("test-event", () => eventsReceived++, { signal });

  info("Emit test-event");
  emitter.emit("test-event");
  equal(eventsReceived, 0, "We didn't receive any event");

  equal(typeof off, "function", "emitter.on still returned a function");
  // check that calling off does not throw
  off();
});

add_task(function testAbortAfterEventListenerIsRemoved() {
  // Check that aborting after there's no more event listener does not throw
  info("Create an EventEmitter");
  const emitter = new EventEmitter();

  const abortController = new AbortController();
  const { signal } = abortController;

  info(
    "Setup an event listener on test-event, controlled by the aborted AbortSignal"
  );
  let eventsReceived = 0;
  const off = emitter.on("test-event", () => eventsReceived++, { signal });

  info("Emit test-event");
  emitter.emit("test-event");
  equal(eventsReceived, 1, "We received the expected event");

  info("Remove the event listener with the function returned by `on`");
  off();

  info("Emit test-event a second time");
  emitter.emit("test-event");
  equal(
    eventsReceived,
    1,
    "We didn't receive new event after removing the event listener"
  );

  info("Abort to check it doesn't throw");
  abortController.abort();
});
