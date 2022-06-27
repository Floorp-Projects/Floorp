/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function run_test() {   
  do_load_manifest("component-blob.manifest");
  const contractID = "@mozilla.org/tests/component-blob;1";
  Assert.ok(contractID in Cc);
  var foo = Cc[contractID]
              .createInstance(Ci.nsIClassInfo);
  Assert.ok(Boolean(foo));
  Assert.ok(foo.contractID == contractID);
  Assert.ok(!!foo.wrappedJSObject);
  Assert.ok(foo.wrappedJSObject.doTest());

}
