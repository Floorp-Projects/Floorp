/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
add_task(function() {
  do_load_manifest("component-file.manifest");
  const contractID = "@mozilla.org/tests/component-file;1";
  Assert.ok(contractID in Components.classes);
  var foo = Components.classes[contractID]
                      .createInstance(Components.interfaces.nsIClassInfo);
  Assert.ok(Boolean(foo));
  Assert.ok(foo.contractID == contractID);
  Assert.ok(!!foo.wrappedJSObject);

  foo.wrappedJSObject.doTest(result => {
    Assert.ok(result);
    run_next_test();
  });
});
