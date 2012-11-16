/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XPConnect Test Code.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;

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
  do_check_true('name' in instances[10], 'Have the prop from A/B');
  do_check_true('someInteger' in instances[10], 'Have the prop from C');

  // Grab tearoff reflections for a and b.
  var aTearOffs = instances.map(function(v, i, a) { return v.nsIXPCTestInterfaceA; } );
  var bTearOffs = instances.map(function(v, i, a) { return v.nsIXPCTestInterfaceB; } );

  // Check
  do_check_true('name' in aTearOffs[1], 'Have the prop from A');
  do_check_true(!('someInteger' in aTearOffs[1]), 'Dont have the prop from C');

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
  do_check_true(!('name' in cTearOffs[1]), 'Dont have the prop from A');
  do_check_true('someInteger' in cTearOffs[1], 'have the prop from C');

  // Null out the a tearoffs.
  aTearOffs = null;

  // Collect all even indices.
  gc();

  // Collect all indices.
  instances = null;
  gc();

  // Give ourselves a pat on the back. :-)
  do_check_true(true, "Got all the way through without crashing!");
}
