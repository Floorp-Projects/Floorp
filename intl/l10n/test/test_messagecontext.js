/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const { FluentBundle, FluentResource } =
    ChromeUtils.import("resource://gre/modules/Fluent.jsm");

  test_methods_presence(FluentBundle);
  test_methods_calling(FluentBundle, FluentResource);

  ok(true);
}

function test_methods_presence(FluentBundle) {
  const bundle = new FluentBundle(["en-US", "pl"]);
  equal(typeof bundle.addResource, "function");
  equal(typeof bundle.formatPattern, "function");
}

function test_methods_calling(FluentBundle, FluentResource) {
  const bundle = new FluentBundle(["en-US", "pl"], {
    useIsolating: false,
  });
  bundle.addResource(new FluentResource("key = Value"));

  const msg = bundle.getMessage("key");
  equal(bundle.formatPattern(msg.value), "Value");

  bundle.addResource(new FluentResource("key2 = Hello { $name }"));

  const msg2 = bundle.getMessage("key2");
  equal(bundle.formatPattern(msg2.value, { name: "Amy" }), "Hello Amy");
  ok(true);
}
