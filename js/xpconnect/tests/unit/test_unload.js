/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function run_test() {
  var scope1 = {};
  var global1 = Components.utils.import("resource://gre/modules/NetUtil.jsm", scope1);

  var scope2 = {};
  var global2 = Components.utils.import("resource://gre/modules/NetUtil.jsm", scope2);

  do_check_true(global1 === global2);
  do_check_true(scope1.NetUtil === scope2.NetUtil);

  Components.utils.unload("resource://gre/modules/NetUtil.jsm");

  var scope3 = {};
  var global3 = Components.utils.import("resource://gre/modules/NetUtil.jsm", scope3);

  do_check_false(global1 === global3);
  do_check_false(scope1.NetUtil === scope3.NetUtil);

  // Both instances should work
  uri1 = scope1.NetUtil.newURI("http://www.example.com");
  do_check_true(uri1 instanceof Components.interfaces.nsIURL);

  var uri3 = scope3.NetUtil.newURI("http://www.example.com");
  do_check_true(uri3 instanceof Components.interfaces.nsIURL);

  do_check_true(uri1.equals(uri3));
}
