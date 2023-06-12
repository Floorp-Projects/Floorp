/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check to see if a weak reference is dead.
let weak_ref_dead = function (r) {
  return !SpecialPowers.nondeterministicGetWeakMapKeys(r).length;
};

add_task(async function cc_xpcwn_dead() {
  // This test demonstrates that a JS reflector for an XPCOM object
  // (implemented via XPCWrappedNative) can be used as a weak map key, but it
  // won't persist across a GC/CC if there are no other references to the key in
  // JS. It would be nice if it did work, in which case we could delete this
  // test, but it would be difficult to implement.

  let wnMap = new WeakMap();

  // Create a new C++ XPCOM container.
  let container = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

  {
    // Create a new C++ XPCOM object, with a new JS reflector.
    let str = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );

    // Set the string data so we can recognize it later.
    str.data = "canary123";

    // Store the C++ object in the C++ container.
    container.appendElement(str);
    is(container.Count(), 1, "The array should have one element");

    // Use the JS reflector as a weak map key.
    wnMap.set(str, {});
    ok(!weak_ref_dead(wnMap), "weak map should have an entry");

    // Make sure there are no references to the JS reflector.
    str = null;
  }

  // Clean up the JS reflector.
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();

  ok(weak_ref_dead(wnMap), "The JS reflector has been freed.");

  // Make a new JS reflector for the C++ XPCOM object.
  let str2 = container.GetElementAt(0).QueryInterface(Ci.nsISupportsString);

  is(str2.data, "canary123", "The C++ object we created still exists.");
});

add_task(async function cc_xpcwn_live() {
  // This test is a slight variation of the previous one. It keeps a reference
  // to the JS reflector for the C++ object, and shows that this keeps it from
  // being removed from the weak map. This is mostly to show why it will work
  // under some conditions.

  let wnMap = new WeakMap();

  // Create a new C++ XPCOM container.
  let container = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

  // Create a new C++ XPCOM object, with a new JS reflector, and hold alive
  // the reflector.
  let str = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );

  // Set the string data so we can recognize it later.
  str.data = "canary345";

  // Store the C++ object in the C++ container.
  container.appendElement(str);
  is(container.Count(), 1, "The array should have one element");

  // Use the JS reflector as a weak map key.
  wnMap.set(str, {});
  ok(!weak_ref_dead(wnMap), "weak map should have an entry");

  // Clean up the JS reflector.
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();

  ok(!weak_ref_dead(wnMap), "The JS reflector hasn't been freed.");

  // Get a JS reflector from scratch for the C++ XPCOM object.
  let str2 = container.GetElementAt(0).QueryInterface(Ci.nsISupportsString);
  is(str, str2, "The JS reflector is the same");
  is(str2.data, "canary345", "The C++ object hasn't changed");
});
