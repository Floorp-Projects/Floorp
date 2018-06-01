/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the LayoutChangesObserver

/* eslint-disable mozilla/use-chromeutils-generateqi */

var {
  getLayoutChangesObserver,
  releaseLayoutChangesObserver,
  LayoutChangesObserver
} = require("devtools/server/actors/reflow");
const EventEmitter = require("devtools/shared/event-emitter");

// Override set/clearTimeout on LayoutChangesObserver to avoid depending on
// time in this unit test. This means that LayoutChangesObserver.eventLoopTimer
// will be the timeout callback instead of the timeout itself, so test cases
// will need to execute it to fake a timeout
LayoutChangesObserver.prototype._setTimeout = cb => cb;
LayoutChangesObserver.prototype._clearTimeout = function() {};

// Mock the tabActor since we only really want to test the LayoutChangesObserver
// and don't want to depend on a window object, nor want to test protocol.js
class MockTabActor extends EventEmitter {
  constructor() {
    super();
    this.window = new MockWindow();
    this.windows = [this.window];
    this.attached = true;
  }
}

function MockWindow() {}
MockWindow.prototype = {
  QueryInterface: function() {
    const self = this;
    return {
      getInterface: function() {
        return {
          QueryInterface: function() {
            if (!self.docShell) {
              self.docShell = new MockDocShell();
            }
            return self.docShell;
          }
        };
      }
    };
  },
  setTimeout: function(cb) {
    // Simply return the cb itself so that we can execute it in the test instead
    // of depending on a real timeout
    return cb;
  },
  clearTimeout: function() {}
};

function MockDocShell() {
  this.observer = null;
}
MockDocShell.prototype = {
  addWeakReflowObserver: function(observer) {
    this.observer = observer;
  },
  removeWeakReflowObserver: function() {},
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
      }
    };
  },
  mockResize: function() {
    if (this.resizeCb) {
      this.resizeCb();
    }
  }
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
  info("Checking that when requesting twice an instances of the observer " +
    "for the same TabActor, the instance is shared");

  info("Checking 2 instances of the observer for the tabActor 1");
  const tabActor1 = new MockTabActor();
  const obs11 = getLayoutChangesObserver(tabActor1);
  const obs12 = getLayoutChangesObserver(tabActor1);
  Assert.equal(obs11, obs12);

  info("Checking 2 instances of the observer for the tabActor 2");
  const tabActor2 = new MockTabActor();
  const obs21 = getLayoutChangesObserver(tabActor2);
  const obs22 = getLayoutChangesObserver(tabActor2);
  Assert.equal(obs21, obs22);

  info("Checking that observers instances for 2 different tabActors are " +
    "different");
  Assert.notEqual(obs11, obs21);

  releaseLayoutChangesObserver(tabActor1);
  releaseLayoutChangesObserver(tabActor1);
  releaseLayoutChangesObserver(tabActor2);
  releaseLayoutChangesObserver(tabActor2);
}

function eventsAreBatched() {
  info("Checking that reflow events are batched and only sent when the " +
    "timeout expires");

  // Note that in this test, we mock the TabActor and its window property, so we
  // also mock the setTimeout/clearTimeout mechanism and just call the callback
  // manually
  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);

  const reflowsEvents = [];
  const onReflows = reflows => reflowsEvents.push(reflows);
  observer.on("reflows", onReflows);

  const resizeEvents = [];
  const onResize = () => resizeEvents.push("resize");
  observer.on("resize", onResize);

  info("Fake one reflow event");
  tabActor.window.docShell.observer.reflow();
  info("Checking that no batched reflow event has been emitted");
  Assert.equal(reflowsEvents.length, 0);

  info("Fake another reflow event");
  tabActor.window.docShell.observer.reflow();
  info("Checking that still no batched reflow event has been emitted");
  Assert.equal(reflowsEvents.length, 0);

  info("Fake a few of resize events too");
  tabActor.window.docShell.mockResize();
  tabActor.window.docShell.mockResize();
  tabActor.window.docShell.mockResize();
  info("Checking that still no batched resize event has been emitted");
  Assert.equal(resizeEvents.length, 0);

  info("Faking timeout expiration and checking that events are sent");
  observer.eventLoopTimer();
  Assert.equal(reflowsEvents.length, 1);
  Assert.equal(reflowsEvents[0].length, 2);
  Assert.equal(resizeEvents.length, 1);

  observer.off("reflows", onReflows);
  observer.off("resize", onResize);
  releaseLayoutChangesObserver(tabActor);
}

function noEventsAreSentWhenThereAreNoReflowsAndLoopTimeouts() {
  info("Checking that if no reflows were detected and the event batching " +
  "loop expires, then no reflows event is sent");

  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);

  const reflowsEvents = [];
  const onReflows = (reflows) => reflowsEvents.push(reflows);
  observer.on("reflows", onReflows);

  info("Faking timeout expiration and checking for reflows");
  observer.eventLoopTimer();
  Assert.equal(reflowsEvents.length, 0);

  observer.off("reflows", onReflows);
  releaseLayoutChangesObserver(tabActor);
}

function observerIsAlreadyStarted() {
  info("Checking that the observer is already started when getting it");

  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);
  Assert.ok(observer.isObserving);

  observer.stop();
  Assert.ok(!observer.isObserving);

  observer.start();
  Assert.ok(observer.isObserving);

  releaseLayoutChangesObserver(tabActor);
}

function destroyStopsObserving() {
  info("Checking that the destroying the observer stops it");

  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);
  Assert.ok(observer.isObserving);

  observer.destroy();
  Assert.ok(!observer.isObserving);

  releaseLayoutChangesObserver(tabActor);
}

function stoppingAndStartingSeveralTimesWorksCorrectly() {
  info("Checking that the stopping and starting several times the observer" +
    " works correctly");

  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);

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

  releaseLayoutChangesObserver(tabActor);
}

function reflowsArentStackedWhenStopped() {
  info("Checking that when stopped, reflows aren't stacked in the observer");

  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);

  info("Stoping the observer");
  observer.stop();

  info("Faking reflows");
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();

  info("Checking that reflows aren't recorded");
  Assert.equal(observer.reflows.length, 0);

  info("Starting the observer and faking more reflows");
  observer.start();
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();

  info("Checking that reflows are recorded");
  Assert.equal(observer.reflows.length, 3);

  releaseLayoutChangesObserver(tabActor);
}

function stackedReflowsAreResetOnStop() {
  info("Checking that stacked reflows are reset on stop");

  const tabActor = new MockTabActor();
  const observer = getLayoutChangesObserver(tabActor);

  tabActor.window.docShell.observer.reflow();
  Assert.equal(observer.reflows.length, 1);

  observer.stop();
  Assert.equal(observer.reflows.length, 0);

  tabActor.window.docShell.observer.reflow();
  Assert.equal(observer.reflows.length, 0);

  observer.start();
  Assert.equal(observer.reflows.length, 0);

  tabActor.window.docShell.observer.reflow();
  Assert.equal(observer.reflows.length, 1);

  releaseLayoutChangesObserver(tabActor);
}
