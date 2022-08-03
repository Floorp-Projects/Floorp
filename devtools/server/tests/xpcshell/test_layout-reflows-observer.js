/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the LayoutChangesObserver

/* eslint-disable mozilla/use-chromeutils-generateqi */

var {
  getLayoutChangesObserver,
  releaseLayoutChangesObserver,
  LayoutChangesObserver,
} = require("devtools/server/actors/reflow");
const EventEmitter = require("devtools/shared/event-emitter");

// Override set/clearTimeout on LayoutChangesObserver to avoid depending on
// time in this unit test. This means that LayoutChangesObserver.eventLoopTimer
// will be the timeout callback instead of the timeout itself, so test cases
// will need to execute it to fake a timeout
LayoutChangesObserver.prototype._setTimeout = cb => cb;
LayoutChangesObserver.prototype._clearTimeout = function() {};

// Mock the targetActor since we only really want to test the LayoutChangesObserver
// and don't want to depend on a window object, nor want to test protocol.js
class MockTargetActor extends EventEmitter {
  constructor() {
    super();
    this.docShell = new MockDocShell();
    this.window = new MockWindow(this.docShell);
    this.windows = [this.window];
    this.attached = true;
  }

  get chromeEventHandler() {
    return this.docShell.chromeEventHandler;
  }

  isDestroyed() {
    return false;
  }
}

function MockWindow(docShell) {
  this.docShell = docShell;
}
MockWindow.prototype = {
  QueryInterface() {
    const self = this;
    return {
      getInterface() {
        return {
          QueryInterface() {
            return self.docShell;
          },
        };
      },
    };
  },
  setTimeout(cb) {
    // Simply return the cb itself so that we can execute it in the test instead
    // of depending on a real timeout
    return cb;
  },
  clearTimeout() {},
};

function MockDocShell() {
  this.observer = null;
}
MockDocShell.prototype = {
  addWeakReflowObserver(observer) {
    this.observer = observer;
  },
  removeWeakReflowObserver() {},
  get chromeEventHandler() {
    return {
      addEventListener: (type, cb) => {
        if (type === "resize") {
          this.resizeCb = cb;
        }
      },
      removeEventListener: (type, cb) => {
        if (type === "resize" && cb === this.resizeCb) {
          this.resizeCb = null;
        }
      },
    };
  },
  mockResize() {
    if (this.resizeCb) {
      this.resizeCb();
    }
  },
};

function run_test() {
  instancesOfObserversAreSharedBetweenWindows();
  eventsAreBatched();
  noEventsAreSentWhenThereAreNoReflowsAndLoopTimeouts();
  observerIsAlreadyStarted();
  destroyStopsObserving();
  stoppingAndStartingSeveralTimesWorksCorrectly();
  reflowsArentStackedWhenStopped();
  stackedReflowsAreResetOnStop();
}

function instancesOfObserversAreSharedBetweenWindows() {
  info(
    "Checking that when requesting twice an instances of the observer " +
      "for the same WindowGlobalTargetActor, the instance is shared"
  );

  info("Checking 2 instances of the observer for the targetActor 1");
  const targetActor1 = new MockTargetActor();
  const obs11 = getLayoutChangesObserver(targetActor1);
  const obs12 = getLayoutChangesObserver(targetActor1);
  Assert.equal(obs11, obs12);

  info("Checking 2 instances of the observer for the targetActor 2");
  const targetActor2 = new MockTargetActor();
  const obs21 = getLayoutChangesObserver(targetActor2);
  const obs22 = getLayoutChangesObserver(targetActor2);
  Assert.equal(obs21, obs22);

  info(
    "Checking that observers instances for 2 different targetActors are " +
      "different"
  );
  Assert.notEqual(obs11, obs21);

  releaseLayoutChangesObserver(targetActor1);
  releaseLayoutChangesObserver(targetActor1);
  releaseLayoutChangesObserver(targetActor2);
  releaseLayoutChangesObserver(targetActor2);
}

function eventsAreBatched() {
  info(
    "Checking that reflow events are batched and only sent when the " +
      "timeout expires"
  );

  // Note that in this test, we mock the target actor and its window property, so we also
  // mock the setTimeout/clearTimeout mechanism and just call the callback manually
  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);

  const reflowsEvents = [];
  const onReflows = reflows => reflowsEvents.push(reflows);
  observer.on("reflows", onReflows);

  const resizeEvents = [];
  const onResize = () => resizeEvents.push("resize");
  observer.on("resize", onResize);

  info("Fake one reflow event");
  targetActor.window.docShell.observer.reflow();
  info("Checking that no batched reflow event has been emitted");
  Assert.equal(reflowsEvents.length, 0);

  info("Fake another reflow event");
  targetActor.window.docShell.observer.reflow();
  info("Checking that still no batched reflow event has been emitted");
  Assert.equal(reflowsEvents.length, 0);

  info("Fake a few of resize events too");
  targetActor.window.docShell.mockResize();
  targetActor.window.docShell.mockResize();
  targetActor.window.docShell.mockResize();
  info("Checking that still no batched resize event has been emitted");
  Assert.equal(resizeEvents.length, 0);

  info("Faking timeout expiration and checking that events are sent");
  observer.eventLoopTimer();
  Assert.equal(reflowsEvents.length, 1);
  Assert.equal(reflowsEvents[0].length, 2);
  Assert.equal(resizeEvents.length, 1);

  observer.off("reflows", onReflows);
  observer.off("resize", onResize);
  releaseLayoutChangesObserver(targetActor);
}

function noEventsAreSentWhenThereAreNoReflowsAndLoopTimeouts() {
  info(
    "Checking that if no reflows were detected and the event batching " +
      "loop expires, then no reflows event is sent"
  );

  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);

  const reflowsEvents = [];
  const onReflows = reflows => reflowsEvents.push(reflows);
  observer.on("reflows", onReflows);

  info("Faking timeout expiration and checking for reflows");
  observer.eventLoopTimer();
  Assert.equal(reflowsEvents.length, 0);

  observer.off("reflows", onReflows);
  releaseLayoutChangesObserver(targetActor);
}

function observerIsAlreadyStarted() {
  info("Checking that the observer is already started when getting it");

  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);
  Assert.ok(observer.isObserving);

  observer.stop();
  Assert.ok(!observer.isObserving);

  observer.start();
  Assert.ok(observer.isObserving);

  releaseLayoutChangesObserver(targetActor);
}

function destroyStopsObserving() {
  info("Checking that the destroying the observer stops it");

  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);
  Assert.ok(observer.isObserving);

  observer.destroy();
  Assert.ok(!observer.isObserving);

  releaseLayoutChangesObserver(targetActor);
}

function stoppingAndStartingSeveralTimesWorksCorrectly() {
  info(
    "Checking that the stopping and starting several times the observer" +
      " works correctly"
  );

  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);

  Assert.ok(observer.isObserving);
  observer.start();
  observer.start();
  observer.start();
  Assert.ok(observer.isObserving);

  observer.stop();
  Assert.ok(!observer.isObserving);

  observer.stop();
  observer.stop();
  Assert.ok(!observer.isObserving);

  releaseLayoutChangesObserver(targetActor);
}

function reflowsArentStackedWhenStopped() {
  info("Checking that when stopped, reflows aren't stacked in the observer");

  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);

  info("Stoping the observer");
  observer.stop();

  info("Faking reflows");
  targetActor.window.docShell.observer.reflow();
  targetActor.window.docShell.observer.reflow();
  targetActor.window.docShell.observer.reflow();

  info("Checking that reflows aren't recorded");
  Assert.equal(observer.reflows.length, 0);

  info("Starting the observer and faking more reflows");
  observer.start();
  targetActor.window.docShell.observer.reflow();
  targetActor.window.docShell.observer.reflow();
  targetActor.window.docShell.observer.reflow();

  info("Checking that reflows are recorded");
  Assert.equal(observer.reflows.length, 3);

  releaseLayoutChangesObserver(targetActor);
}

function stackedReflowsAreResetOnStop() {
  info("Checking that stacked reflows are reset on stop");

  const targetActor = new MockTargetActor();
  const observer = getLayoutChangesObserver(targetActor);

  targetActor.window.docShell.observer.reflow();
  Assert.equal(observer.reflows.length, 1);

  observer.stop();
  Assert.equal(observer.reflows.length, 0);

  targetActor.window.docShell.observer.reflow();
  Assert.equal(observer.reflows.length, 0);

  observer.start();
  Assert.equal(observer.reflows.length, 0);

  targetActor.window.docShell.observer.reflow();
  Assert.equal(observer.reflows.length, 1);

  releaseLayoutChangesObserver(targetActor);
}
