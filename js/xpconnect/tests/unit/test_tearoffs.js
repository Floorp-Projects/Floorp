/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {

  // Load the component manifest containing our test interface implementations.
  Components.manager.autoRegister(do_get_file('../components/js/xpctest.manifest'));

  // Shortcut the interfaces we're using.
  var ifs = {
    a: Ci['nsIXPCTestInterfaceA'],
    b: Ci['nsIXPCTestInterfaceB'],
    c: Ci['nsIXPCTestInterfaceC']
  };

  // Shortcut the class we're instantiating. This implements all three interfaces.
  var cls = Cc["@mozilla.org/js/xpc/test/js/TestInterfaceAll;1"];

  // Run through the logic a few times.
  for (i = 0; i < 2; ++i)
    play_with_tearoffs(ifs, cls);
}

function play_with_tearoffs(ifs, cls) {

  // Allocate a bunch of objects, QI-ed to B.
  var instances = [];
  for (var i = 0; i < 300; ++i)
    instances.push(cls.createInstance(ifs.b));

  // Nothing to collect.
  gc();

  // QI them to A.
  instances.forEach(function(v, i, a) { v.QueryInterface(ifs.a); });

  // QI them to C.
  instances.forEach(function(v, i, a) { v.QueryInterface(ifs.c); });

  // Check
  Assert.ok('name' in instances[10], 'Have the prop from A/B');
  Assert.ok('someInteger' in instances[10], 'Have the prop from C');

  // Grab tearoff reflections for a and b.
  var aTearOffs = instances.map(function(v, i, a) { return v.nsIXPCTestInterfaceA; } );
  var bTearOffs = instances.map(function(v, i, a) { return v.nsIXPCTestInterfaceB; } );

  // Check
  Assert.ok('name' in aTearOffs[1], 'Have the prop from A');
  Assert.ok(!('someInteger' in aTearOffs[1]), 'Dont have the prop from C');

  // Nothing to collect.
  gc();

  // Null out every even instance pointer.
  for (var i = 0; i < instances.length; ++i)
    if (i % 2 == 0)
        instances[i] = null;

  // Nothing to collect, since we still have the A and B tearoff reflections.
  gc();

  // Null out A tearoff reflections that are a multiple of 3.
  for (var i = 0; i < aTearOffs.length; ++i)
    if (i % 3 == 0)
        aTearOffs[i] = null;

  // Nothing to collect, since we still have the B tearoff reflections.
  gc();

  // Null out B tearoff reflections that are a multiple of 5.
  for (var i = 0; i < bTearOffs.length; ++i)
    if (i % 5 == 0)
        bTearOffs[i] = null;

  // This should collect every 30th object (indices that are multiples of 2, 3, and 5).
  gc();

  // Kill the b tearoffs entirely.
  bTearOffs = 0;

  // Collect more.
  gc();

  // Get C tearoffs.
  var cTearOffs = instances.map(function(v, i, a) { return v ? v.nsIXPCTestInterfaceC : null; } );

  // Check.
  Assert.ok(!('name' in cTearOffs[1]), 'Dont have the prop from A');
  Assert.ok('someInteger' in cTearOffs[1], 'have the prop from C');

  // Null out the a tearoffs.
  aTearOffs = null;

  // Collect all even indices.
  gc();

  // Collect all indices.
  instances = null;
  gc();

  // Give ourselves a pat on the back. :-)
  Assert.ok(true, "Got all the way through without crashing!");
}
