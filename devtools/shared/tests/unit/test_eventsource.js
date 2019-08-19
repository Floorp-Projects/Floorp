/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const eventSource = require("devtools/shared/client/event-source");

// Test basic event-source APIs:
// - addListener
// - removeListener
// - addOneTimeListener

add_task(function() {
  // Create a basic test object that can emit events using event-source.js
  class TestClass {}
  eventSource(TestClass.prototype);
  const testObject = new TestClass();

  testBasicAddRemoveListener(testObject);

  info("Check that one time listeners are only triggered once");
  testOneTimeListener(testObject);

  info("Check that one time listeners can be removed");
  testRemoveOneTimeListener(testObject);
});

function testBasicAddRemoveListener(testObject) {
  let eventsReceived = 0;
  const onTestEvent = () => eventsReceived++;

  testObject.addListener("event-testBasicAddRemoveListener", onTestEvent);
  testObject.emit("event-testBasicAddRemoveListener");
  ok(eventsReceived === 1, "Event listener was triggered");

  testObject.emit("event-testBasicAddRemoveListener");
  ok(eventsReceived === 2, "Event listener was triggered again");

  testObject.removeListener("event-testBasicAddRemoveListener", onTestEvent);
  testObject.emit("event-testBasicAddRemoveListener");
  ok(eventsReceived === 2, "Event listener was not triggered anymore");
}

function testOneTimeListener(testObject) {
  let eventsReceived = 0;
  const onTestEvent = () => eventsReceived++;

  testObject.addOneTimeListener("event-testOneTimeListener", onTestEvent);
  testObject.emit("event-testOneTimeListener");
  ok(eventsReceived === 1, "Event listener was triggered");

  testObject.emit("event-testOneTimeListener");
  ok(eventsReceived === 1, "Event listener was not triggered again");

  testObject.removeListener("event-testOneTimeListener", onTestEvent);
}

function testRemoveOneTimeListener(testObject) {
  let eventsReceived = 0;
  const onTestEvent = () => eventsReceived++;

  testObject.addOneTimeListener("event-testRemoveOneTimeListener", onTestEvent);
  testObject.removeListener("event-testRemoveOneTimeListener", onTestEvent);
  testObject.emit("event-testRemoveOneTimeListener");
  ok(eventsReceived === 0, "Event listener was already removed");
}
