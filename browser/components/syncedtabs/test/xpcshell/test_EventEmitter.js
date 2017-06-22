"use strict";

let { EventEmitter } = Cu.import("resource:///modules/syncedtabs/EventEmitter.jsm", {});

add_task(async function testSingleListener() {
  let eventEmitter = new EventEmitter();
  let spy = sinon.spy();

  eventEmitter.on("click", spy);
  eventEmitter.emit("click", "foo", "bar");
  Assert.ok(spy.calledOnce);
  Assert.ok(spy.calledWith("foo", "bar"));

  eventEmitter.off("click", spy);
  eventEmitter.emit("click");
  Assert.ok(spy.calledOnce);
});

add_task(async function testMultipleListeners() {
  let eventEmitter = new EventEmitter();
  let spy1 = sinon.spy();
  let spy2 = sinon.spy();

  eventEmitter.on("some_event", spy1);
  eventEmitter.on("some_event", spy2);
  eventEmitter.emit("some_event");
  Assert.ok(spy1.calledOnce);
  Assert.ok(spy2.calledOnce);

  eventEmitter.off("some_event", spy1);
  eventEmitter.emit("some_event");
  Assert.ok(spy1.calledOnce);
  Assert.ok(spy2.calledTwice);
});

