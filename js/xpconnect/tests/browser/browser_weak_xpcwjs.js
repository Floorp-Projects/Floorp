/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Some basic tests of the lifetime of an XPCWJS with a weak reference.

// Create a weak reference, with a single-element weak map.
let make_weak_ref = function (obj) {
  let m = new WeakMap();
  m.set(obj, {});
  return m;
};

// Check to see if a weak reference is dead.
let weak_ref_dead = function (r) {
  return !SpecialPowers.nondeterministicGetWeakMapKeys(r).length;
};

add_task(async function gc_wwjs() {
  // This subtest checks that a WJS with only a weak reference to it gets
  // cleaned up, if its JS object is garbage, after just a GC.
  // For the browser, this probably isn't important, but tests seem to rely
  // on it.
  const TEST_PREF = "wjs.pref1";
  let wjs_weak_ref = null;
  let observed_count = 0;

  {
    Services.prefs.clearUserPref(TEST_PREF);

    // Create the observer object.
    let observer1 = {
      QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),
      observe() {
        observed_count += 1;
        info(TEST_PREF + " pref observer.");
      },
    };

    // Register the weak observer.
    Services.prefs.addObserver(TEST_PREF, observer1, true);

    // Invoke the observer to make sure it is doing something.
    info("Flipping the pref " + TEST_PREF);
    Services.prefs.setBoolPref(TEST_PREF, true);
    is(observed_count, 1, "Ran observer1 once after first flip.");

    wjs_weak_ref = make_weak_ref(observer1);

    // Exit the scope, making observer1 garbage.
  }

  // Run the GC.
  info("Running the GC.");
  SpecialPowers.forceGC();

  // Flip the pref again to make sure that the observer doesn't run.
  info("Flipping the pref " + TEST_PREF);
  Services.prefs.setBoolPref(TEST_PREF, false);

  is(observed_count, 1, "After GC, don't run the observer.");
  ok(weak_ref_dead(wjs_weak_ref), "WJS with weak ref should be freed.");

  Services.prefs.clearUserPref(TEST_PREF);
});

add_task(async function alive_wwjs() {
  // This subtest checks that a WJS with only a weak reference should not get
  // cleaned up if the underlying JS object is held alive (here, via the
  // variable |observer2|).
  const TEST_PREF = "wjs.pref2";
  let observed_count = 0;

  Services.prefs.clearUserPref(TEST_PREF);
  let observer2 = {
    QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),
    observe() {
      observed_count += 1;
      info(TEST_PREF + " pref observer");
    },
  };
  Services.prefs.addObserver(TEST_PREF, observer2, true);

  Services.prefs.setBoolPref(TEST_PREF, true);
  is(observed_count, 1, "Run observer2 once after first flip.");

  await new Promise(resolve =>
    SpecialPowers.exactGC(() => {
      SpecialPowers.forceCC();
      SpecialPowers.forceGC();
      SpecialPowers.forceCC();

      Services.prefs.setBoolPref(TEST_PREF, false);

      is(observed_count, 2, "Run observer2 again after second flip.");

      Services.prefs.removeObserver(TEST_PREF, observer2);
      Services.prefs.clearUserPref(TEST_PREF);

      resolve();
    })
  );
});

add_task(async function cc_wwjs() {
  // This subtest checks that a WJS with only a weak reference to it, where the
  // underlying JS object is part of a garbage cycle, gets cleaned up after a
  // cycle collection. It also checks that things held alive by the JS object
  // don't end up in an unlinked state, although that's mostly for fun, because
  // it is redundant with checking that the JS object gets cleaned up.
  const TEST_PREF = "wjs.pref3";
  let wjs_weak_ref = null;
  let observed_count = 0;
  let canary_count;

  {
    Services.prefs.clearUserPref(TEST_PREF);

    // Set up a canary object that lets us detect unlinking.
    // (When an nsArrayCC is unlinked, all of the elements are removed.)
    // This is needed to distinguish the case where the observer was unlinked
    // without removing the weak reference from the case where we did not
    // collect the observer at all.
    let canary = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let someString = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    someString.data = "canary";
    canary.appendElement(someString);
    canary.appendElement(someString);
    is(canary.Count(), 2, "The canary array should have two elements");

    // Create the observer object.
    let observer3 = {
      QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),
      canary,
      cycle: new DOMMatrix(),
      observe() {
        observed_count += 1;
        canary_count = this.canary.Count();
        info(TEST_PREF + " pref observer. Canary count: " + canary_count);
      },
    };

    // Set up a cycle between C++ and JS that requires the CC to collect.
    // |cycle| is a random WebIDL object that we can set an expando on to
    // create a nice clean cycle that doesn't involve any weird XPConnect stuff.
    observer3.cycle.backEdge = observer3;

    // Register the weak observer.
    Services.prefs.addObserver(TEST_PREF, observer3, true);

    // Invoke the observer to make sure it is doing something.
    info("Flipping the pref " + TEST_PREF);
    canary_count = -1;
    Services.prefs.setBoolPref(TEST_PREF, true);
    is(
      canary_count,
      2,
      "Observer ran with expected value while observer3 is alive."
    );
    is(observed_count, 1, "Ran observer3 once after first flip.");

    wjs_weak_ref = make_weak_ref(observer3);

    // Exit the scope, making observer3 and canary garbage.
  }

  // Run the GC. This is necessary to mark observer3 gray so the CC
  // might consider it to be garbage. This won't free it because it is held
  // alive from C++ (namely the DOMMatrix via its expando).
  info("Running the GC.");
  SpecialPowers.forceGC();

  // Note: Don't flip the pref here. Doing so will run the observer, which will
  // cause it to get marked black again, preventing it from being freed.
  // For the same reason, don't call weak_ref_dead(wjs_weak_ref) here.

  // Run the CC. This should detect that the cycle between observer3 and the
  // DOMMatrix is garbage, unlinking the DOMMatrix and the canary. Also, the
  // weak reference for the WJS for observer3 should get cleared because the
  // underlying JS object has been identifed as garbage. You can add logging to
  // nsArrayCC's unlink method to see the canary getting unlinked.
  info("Running the CC.");
  SpecialPowers.forceCC();

  // Flip the pref again to make sure that the observer doesn't run.
  info("Flipping the pref " + TEST_PREF);
  canary_count = -1;
  Services.prefs.setBoolPref(TEST_PREF, false);

  isnot(
    canary_count,
    0,
    "After CC, don't run the observer with an unlinked canary."
  );
  isnot(
    canary_count,
    2,
    "After CC, don't run the observer after it is garbage."
  );
  is(canary_count, -1, "After CC, don't run the observer.");
  is(observed_count, 1, "After CC, don't run the observer.");

  ok(
    !weak_ref_dead(wjs_weak_ref),
    "WJS with weak ref shouldn't be freed by the CC."
  );

  // Now that the CC has identified observer3 as garbage, running the GC again
  // should free it.
  info("Running the GC again.");
  SpecialPowers.forceGC();

  ok(weak_ref_dead(wjs_weak_ref), "WJS with weak ref should be freed.");

  info("Flipping the pref " + TEST_PREF);
  canary_count = -1;
  Services.prefs.setBoolPref(TEST_PREF, true);

  // Note: the original implementation of weak references for WJS fails most of
  // the prior canary_count tests, but passes these.
  isnot(
    canary_count,
    0,
    "After GC, don't run the observer with an unlinked canary."
  );
  isnot(
    canary_count,
    2,
    "After GC, don't run the observer after it is garbage."
  );
  is(canary_count, -1, "After GC, don't run the observer.");
  is(observed_count, 1, "After GC, don't run the observer.");

  Services.prefs.clearUserPref(TEST_PREF);
});
