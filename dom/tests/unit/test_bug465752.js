/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test()
{
  const ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
  const str = "javascript:10";
  var uri = ios.newURI(str, null, null);
  var uri2 = ios.newURI(str, null, null);
  const str2 = "http://example.org";
  var uri3 = ios.newURI(str2, null, null);
  do_check_true(uri.equals(uri));
  do_check_true(uri.equals(uri2));
  do_check_true(uri2.equals(uri));
  do_check_true(uri2.equals(uri2));
  do_check_false(uri3.equals(uri2));
  do_check_false(uri2.equals(uri3));

  var simple = Components.classes["@mozilla.org/network/simple-uri;1"]
                         .createInstance(Components.interfaces.nsIURI);
  simple.spec = str;
  do_check_eq(simple.spec, uri.spec);
  do_check_false(simple.equals(uri));
  do_check_false(uri.equals(simple));
}
