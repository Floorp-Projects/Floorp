/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const { FluentBundle } = ChromeUtils.import("resource://gre/modules/Fluent.jsm", {});

  test_methods_presence(FluentBundle);
  test_methods_calling(FluentBundle);

  ok(true);
}

function test_methods_presence(FluentBundle) {
  const bundle = new FluentBundle(["en-US", "pl"]);
  equal(typeof bundle.addMessages, "function");
  equal(typeof bundle.format, "function");
}

function test_methods_calling(FluentBundle) {
  const bundle = new FluentBundle(["en-US", "pl"], {
    useIsolating: false,
  });
  bundle.addMessages("key = Value");

  const msg = bundle.getMessage("key");
  equal(bundle.format(msg), "Value");

  bundle.addMessages("key2 = Hello { $name }");

  const msg2 = bundle.getMessage("key2");
  equal(bundle.format(msg2, { name: "Amy" }), "Hello Amy");
  ok(true);
}
