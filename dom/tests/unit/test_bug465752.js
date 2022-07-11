/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  const str = "javascript:10";
  var uri = Services.io.newURI(str);
  var uri2 = Services.io.newURI(str);
  const str2 = "http://example.org";
  var uri3 = Services.io.newURI(str2);
  Assert.ok(uri.equals(uri));
  Assert.ok(uri.equals(uri2));
  Assert.ok(uri2.equals(uri));
  Assert.ok(uri2.equals(uri2));
  Assert.ok(!uri3.equals(uri2));
  Assert.ok(!uri2.equals(uri3));

  var simple = Cc["@mozilla.org/network/simple-uri-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec(str)
    .finalize();
  Assert.equal(simple.spec, uri.spec);
  Assert.ok(!simple.equals(uri));
  Assert.ok(!uri.equals(simple));
}
