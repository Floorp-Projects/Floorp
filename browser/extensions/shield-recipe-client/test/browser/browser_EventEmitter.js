"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Console.jsm", this);
Cu.import("resource://shield-recipe-client/lib/EventEmitter.jsm", this);

const evidence = {
  a: 0,
  b: 0,
  c: 0,
  log: "",
};

function listenerA(x = 1) {
  evidence.a += x;
  evidence.log += "a";
}

function listenerB(x = 1) {
  evidence.b += x;
  evidence.log += "b";
}

function listenerC(x = 1) {
  evidence.c += x;
  evidence.log += "c";
}

add_task(withSandboxManager(Assert, function* (sandboxManager) {
  const eventEmitter = new EventEmitter(sandboxManager);

  // Fire an unrelated event, to make sure nothing goes wrong
  eventEmitter.on("nothing");

  // bind listeners
  eventEmitter.on("event", listenerA);
  eventEmitter.on("event", listenerB);
  eventEmitter.once("event", listenerC);

  // one event for all listeners
  eventEmitter.emit("event");
  // another event for a and b, since c should have turned off already
  eventEmitter.emit("event", 10);

  // make sure events haven't actually fired yet, just queued
  Assert.deepEqual(evidence, {
    a: 0,
    b: 0,
    c: 0,
    log: "",
  }, "events are fired async");

  // Spin the event loop to run events, so we can safely "off"
  yield Promise.resolve();

  // Check intermediate event results
  Assert.deepEqual(evidence, {
    a: 11,
    b: 11,
    c: 1,
    log: "abcab",
  }, "intermediate events are fired");

  // one more event for a
  eventEmitter.off("event", listenerB);
  eventEmitter.emit("event", 100);

  // And another unrelated event
  eventEmitter.on("nothing");

  // Spin the event loop to run events
  yield Promise.resolve();

  Assert.deepEqual(evidence, {
    a: 111,
    b: 11,
    c: 1,
    log: "abcaba",  // events are in order
  }, "events fired as expected");

  // Test that mutating the data passed to the event doesn't actually
  // mutate it for other events.
  let handlerRunCount = 0;
  const mutationHandler = data => {
    handlerRunCount++;
    data.count++;
    is(data.count, 1, "Event data is not mutated between handlers.");
  };
  eventEmitter.on("mutationTest", mutationHandler);
  eventEmitter.on("mutationTest", mutationHandler);

  const data = {count: 0};
  eventEmitter.emit("mutationTest", data);
  yield Promise.resolve();

  is(handlerRunCount, 2, "Mutation handler was executed twice.");
  is(data.count, 0, "Event data cannot be mutated by handlers.");
}));

add_task(withSandboxManager(Assert, function* sandboxedEmitter(sandboxManager) {
  const eventEmitter = new EventEmitter(sandboxManager);

  // Event handlers inside the sandbox should be run in response to
  // events triggered outside the sandbox.
  sandboxManager.addGlobal("emitter", eventEmitter.createSandboxedEmitter());
  sandboxManager.evalInSandbox(`
    this.eventCounts = {on: 0, once: 0};
    emitter.on("event", value => {
      this.eventCounts.on += value;
    });
    emitter.once("eventOnce", value => {
      this.eventCounts.once += value;
    });
  `);

  eventEmitter.emit("event", 5);
  eventEmitter.emit("event", 10);
  eventEmitter.emit("eventOnce", 5);
  eventEmitter.emit("eventOnce", 10);
  yield Promise.resolve();

  const eventCounts = sandboxManager.evalInSandbox("this.eventCounts");
  Assert.deepEqual(eventCounts, {
    on: 15,
    once: 5,
  }, "Events emitted outside a sandbox trigger handlers within a sandbox.");
}));
