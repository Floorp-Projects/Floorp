/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=898559 */

function run_test()
{
  let sandbox = Components.utils.Sandbox("http://www.blah.com", {
    metadata: "test metadata",
    addonId: "12345"
  });

  do_check_eq(Components.utils.getSandboxMetadata(sandbox), "test metadata");
  do_check_eq(Components.utils.getSandboxAddonId(sandbox), "12345");

  sandbox = Components.utils.Sandbox("http://www.blah.com", {
    metadata: { foopy: { bar: 2 }, baz: "hi" }
  });

  let metadata = Components.utils.getSandboxMetadata(sandbox);
  do_check_eq(metadata.baz, "hi");
  do_check_eq(metadata.foopy.bar, 2);
  metadata.baz = "foo";

  metadata = Components.utils.getSandboxMetadata(sandbox);
  do_check_eq(metadata.baz, "foo");

  metadata = { foo: "bar" };
  Components.utils.setSandboxMetadata(sandbox, metadata);
  metadata.foo = "baz";
  metadata = Components.utils.getSandboxMetadata(sandbox);
  do_check_eq(metadata.foo, "bar");

  let thrown = false;
  let reflector = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                    .createInstance(Components.interfaces.nsIXMLHttpRequest);

  try {
    Components.utils.setSandboxMetadata(sandbox, { foo: reflector });
  } catch(e) {
    thrown = true;
  }

  do_check_eq(thrown, true);

  sandbox = Components.utils.Sandbox(this, {
    metadata: { foopy: { bar: 2 }, baz: "hi" }
  });

  let inner = Components.utils.evalInSandbox("Components.utils.Sandbox('http://www.blah.com')", sandbox);

  metadata = Components.utils.getSandboxMetadata(inner);
  do_check_eq(metadata.baz, "hi");
  do_check_eq(metadata.foopy.bar, 2);
  metadata.baz = "foo";
}

