"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://shield-recipe-client/lib/NormandyDriver.jsm", this);
Cu.import("resource://shield-recipe-client/lib/SandboxManager.jsm", this);

const sandboxManager = new SandboxManager();
sandboxManager.addHold("test running");
const driver = new NormandyDriver(sandboxManager);
const sandboxedDriver = Cu.cloneInto(driver, sandboxManager.sandbox, {cloneFunctions: true});
const eventEmitter = new sandboxManager.sandbox.EventEmitter(sandboxedDriver).wrappedJSObject;


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

add_task(function* () {
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

  sandboxManager.removeHold("test running");

  yield sandboxManager.isNuked()
    .then(() => ok(true, "sandbox is nuked"))
    .catch(e => ok(false, "sandbox is nuked", e));
});
