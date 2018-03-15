/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=898559 */

function run_test()
{
  let sandbox = Cu.Sandbox("http://www.blah.com", {
    metadata: "test metadata",
  });

  Cu.importGlobalProperties(["XMLHttpRequest"]);

  Assert.equal(Cu.getSandboxMetadata(sandbox), "test metadata");

  sandbox = Cu.Sandbox("http://www.blah.com", {
    metadata: { foopy: { bar: 2 }, baz: "hi" }
  });

  let metadata = Cu.getSandboxMetadata(sandbox);
  Assert.equal(metadata.baz, "hi");
  Assert.equal(metadata.foopy.bar, 2);
  metadata.baz = "foo";

  metadata = Cu.getSandboxMetadata(sandbox);
  Assert.equal(metadata.baz, "foo");

  metadata = { foo: "bar" };
  Cu.setSandboxMetadata(sandbox, metadata);
  metadata.foo = "baz";
  metadata = Cu.getSandboxMetadata(sandbox);
  Assert.equal(metadata.foo, "bar");

  let thrown = false;
  let reflector = new XMLHttpRequest();

  try {
    Cu.setSandboxMetadata(sandbox, { foo: reflector });
  } catch(e) {
    thrown = true;
  }

  Assert.equal(thrown, true);

  sandbox = Cu.Sandbox(this, {
    metadata: { foopy: { bar: 2 }, baz: "hi" }
  });

  let inner = Cu.evalInSandbox("Components.utils.Sandbox('http://www.blah.com')", sandbox);

  metadata = Cu.getSandboxMetadata(inner);
  Assert.equal(metadata.baz, "hi");
  Assert.equal(metadata.foopy.bar, 2);
  metadata.baz = "foo";
}

