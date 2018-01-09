/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test()
{
  const ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
  const str = "javascript:10";
  var uri = ios.newURI(str);
  var uri2 = ios.newURI(str);
  const str2 = "http://example.org";
  var uri3 = ios.newURI(str2);
  Assert.ok(uri.equals(uri));
  Assert.ok(uri.equals(uri2));
  Assert.ok(uri2.equals(uri));
  Assert.ok(uri2.equals(uri2));
  Assert.ok(!uri3.equals(uri2));
  Assert.ok(!uri2.equals(uri3));

  var simple = Components.classes["@mozilla.org/network/simple-uri-mutator;1"]
                         .createInstance(Components.interfaces.nsIURIMutator)
                         .setSpec(str)
                         .finalize();
  Assert.equal(simple.spec, uri.spec);
  Assert.ok(!simple.equals(uri));
  Assert.ok(!uri.equals(simple));
}
