/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetter(this, 'setTimeout',
  'resource://gre/modules/Timer.jsm');

// Setup logging prefs before importing the scheduler module.
Services.prefs.setCharPref("readinglist.log.appender.dump", "Trace");

let {createTestableScheduler} = Cu.import("resource:///modules/readinglist/Scheduler.jsm", {});
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

// Log rotation needs a profile dir.
do_get_profile();

let prefs = new Preferences("readinglist.scheduler.");
prefs.set("enabled", true);

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (subject, topic, data) => {
      Services.obs.removeObserver(obs, topic);
      resolve(data);
    }
    Services.obs.addObserver(obs, topic, false);
  });
}

function ReadingListMock() {
  this.listener = null;
}

ReadingListMock.prototype = {
  addListener(listener) {
    ok(!this.listener, "mock only expects 1 listener");
    this.listener = listener;
  },
}

function createScheduler(options) {
  // avoid typos in the test and other footguns in the options.
  let allowedOptions = ["expectedDelay", "expectNewTimer", "syncFunction"];
  for (let key of Object.keys(options)) {
    if (allowedOptions.indexOf(key) == -1) {
      throw new Error("Invalid option " + key);
    }
  }
  let rlMock = new ReadingListMock();
  let scheduler = createTestableScheduler(rlMock);
  // make our hooks
  let syncFunction = options.syncFunction || Promise.resolve;
  scheduler._engine.start = syncFunction;
  // we expect _setTimeout to be called *twice* - first is the initial sync,
  // and there's no need to test the delay used for that. options.expectedDelay
  // is to check the *subsequent* timer.
  let numCalls = 0;
  scheduler._setTimeout = function(delay) {
    ++numCalls;
    print("Test scheduler _setTimeout call number " + numCalls + " with delay=" + delay);
    switch (numCalls) {
      case 1:
        // this is the first and boring schedule as it initializes - do nothing
        // other than return a timer that fires immediately.
        return setTimeout(() => scheduler._doSync(), 0);
        break;
      case 2:
        // This is the one we are interested in, so check things.
        if (options.expectedDelay) {
          // a little slop is OK as it takes a few ms to actually set the timer
          ok(Math.abs(options.expectedDelay * 1000 - delay) < 500, [options.expectedDelay * 1000, delay]);
        }
        // and return a timeout that "never" fires
        return setTimeout(() => scheduler._doSync(), 10000000);
        break;
      default:
        // This is unexpected!
        ok(false, numCalls);
    }
  };
  // And a callback made once we've determined the next delay. This is always
  // called even if _setTimeout isn't (due to no timer being created)
  scheduler._onAutoReschedule = () => {
    // Most tests expect a new timer, so this is "opt out"
    let expectNewTimer = options.expectNewTimer === undefined ? true : options.expectNewTimer;
    ok(expectNewTimer ? scheduler._timer : !scheduler._timer);
  }
  // calling .init fires things off...
  scheduler.init();
  return scheduler;
}

add_task(function* testSuccess() {
  // promises which resolve once we've got all the expected notifications.
  let allNotifications = [
    promiseObserver("readinglist:sync:start"),
    promiseObserver("readinglist:sync:finish"),
  ];
  // New delay should be "as regularly scheduled".
  prefs.set("schedule", 100);
  let scheduler = createScheduler({expectedDelay: 100});
  yield Promise.all(allNotifications);
  scheduler.finalize();
});

// Test that if we get a reading list notification while we are syncing we
// immediately start a new one when it complets.
add_task(function* testImmediateResyncWhenChangedDuringSync() {
  // promises which resolve once we've got all the expected notifications.
  let allNotifications = [
    promiseObserver("readinglist:sync:start"),
    promiseObserver("readinglist:sync:finish"),
  ];
  prefs.set("schedule", 100);
  // New delay should be "immediate".
  let scheduler = createScheduler({
    expectedDelay: 0,
    syncFunction: () => {
      // we are now syncing - pretend the readinglist has an item change
      scheduler.readingList.listener.onItemAdded();
      return Promise.resolve();
    }});
  yield Promise.all(allNotifications);
  scheduler.finalize();
});

add_task(function* testOffline() {
  let scheduler = createScheduler({expectNewTimer: false});
  Services.io.offline = true;
  ok(!scheduler._canSync(), "_canSync is false when offline.")
  ok(!scheduler._timer, "there is no current timer while offline.")
  Services.io.offline = false;
  ok(scheduler._canSync(), "_canSync is true when online.")
  ok(scheduler._timer, "there is a new timer when back online.")
  scheduler.finalize();
});

add_task(function* testRetryableError() {
  let allNotifications = [
    promiseObserver("readinglist:sync:start"),
    promiseObserver("readinglist:sync:error"),
  ];
  prefs.set("retry", 10);
  let scheduler = createScheduler({
    expectedDelay: 10,
    syncFunction: () => Promise.reject("transient"),
  });
  yield Promise.all(allNotifications);
  scheduler.finalize();
});

add_task(function* testAuthError() {
  prefs.set("retry", 10);
  // We expect an auth error to result in no new timer (as it's waiting for
  // some indication it can proceed), but with the next delay being a normal
  // "retry" interval (so when we can proceed it is probably already stale, so
  // is effectively "immediate")
  let scheduler = createScheduler({
    expectedDelay: 10,
    syncFunction:  () => {
      return Promise.reject(ReadingListScheduler._engine.ERROR_AUTHENTICATION);
    },
    expectNewTimer: false
  });
  // XXX - TODO - send an observer that "unblocks" us and ensure we actually
  // do unblock.
  scheduler.finalize();
});

add_task(function* testBackoff() {
  let scheduler = createScheduler({expectedDelay: 1000});
  Services.obs.notifyObservers(null, "readinglist:backoff-requested", 1000);
  // XXX - this needs a little love as nothing checks createScheduler actually
  // made the checks we think it does.
  scheduler.finalize();
});

add_task(function testErrorBackoff() {
  // This test can't sanely use the "test scheduler" above, so make one more
  // suited.
  let rlMock = new ReadingListMock();
  let scheduler = createTestableScheduler(rlMock);
  scheduler._setTimeout = function(delay) {
    // create a timer that fires immediately
    return setTimeout(() => scheduler._doSync(), 0);
  }

  // This does all the work...
  function checkBackoffs(expectedSequences) {
    let orig_maybeReschedule = scheduler._maybeReschedule;
    return new Promise(resolve => {
      let isSuccess = true; // ie, first run will put us in "fail" mode.
      let expected;
      function nextSequence() {
        if (expectedSequences.length == 0) {
          resolve();
          return true; // we are done.
        }
        // setup the current set of expected results.
        expected = expectedSequences.shift()
        // and toggle the success status of the engine.
        isSuccess = !isSuccess;
        if (isSuccess) {
          scheduler._engine.start = Promise.resolve;
        } else {
          scheduler._engine.start = () => {
            return Promise.reject(new Error("oh no"))
          }
        }
        return false; // not done.
      };
      // get the first sequence;
      nextSequence();
      // and setup the scheduler to check the sequences.
      scheduler._maybeReschedule = function(nextDelay) {
        let thisExpected = expected.shift();
        equal(thisExpected * 1000, nextDelay);
        if (expected.length == 0) {
          if (nextSequence()) {
            // we are done, so do nothing.
            return;
          }
        }
        // call the original impl to get the next schedule.
        return orig_maybeReschedule.call(scheduler, nextDelay);
      }
    });
  }

  prefs.set("schedule", 100);
  prefs.set("retry", 5);
  // The sequences of timeouts we expect as the Sync error state changes.
  let backoffsChecked = checkBackoffs([
    // first sequence is in failure mode - expect the timeout to double until 'schedule'
    [5, 10, 20, 40, 80, 100, 100],
    // Sync just started working - more 'schedule'
    [100, 100],
    // Just stopped again - error backoff process restarts.
    [5, 10],
    // Another success and we are back to 'schedule'
    [100, 100],
  ]);

  // fire things off.
  scheduler.init();

  // and wait for completion.
  yield backoffsChecked;

  scheduler.finalize();
});

function run_test() {
  run_next_test();
}
