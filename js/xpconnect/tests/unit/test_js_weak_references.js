/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=317304 */

function run_test()
{
  // Bug 712649: Calling getWeakReference(null) should work.
  try {
    var nullWeak = Components.utils.getWeakReference(null);
    Assert.ok(nullWeak.get() === null);
  } catch (e) {
    Assert.ok(false);
  }

  var obj = { num: 5, str: 'foo' };
  var weak = Components.utils.getWeakReference(obj);

  Assert.ok(weak.get() === obj);
  Assert.ok(weak.get().num == 5);
  Assert.ok(weak.get().str == 'foo');

  // Force garbage collection
  Components.utils.forceGC();

  // obj still references the object, so it should still be accessible via weak
  Assert.ok(weak.get() === obj);
  Assert.ok(weak.get().num == 5);
  Assert.ok(weak.get().str == 'foo');

  // Clear obj's reference to the object and force garbage collection. To make
  // sure that there are no instances of obj stored in the registers or on the
  // native stack and the conservative GC would not find it we force the same
  // code paths that we used for the initial allocation.
  obj = { num: 6, str: 'foo2' };
  var weak2 = Components.utils.getWeakReference(obj);
  Assert.ok(weak2.get() === obj);

  Components.utils.forceGC();

  // The object should have been garbage collected and so should no longer be
  // accessible via weak
  Assert.ok(weak.get() === null);
}
