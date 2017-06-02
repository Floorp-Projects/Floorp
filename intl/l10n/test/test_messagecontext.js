/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const { MessageContext } = Components.utils.import("resource://gre/modules/MessageContext.jsm", {});

  test_methods_presence(MessageContext);
  test_methods_calling(MessageContext);

  ok(true);
}

function test_methods_presence(MessageContext) {
  const ctx = new MessageContext(["en-US", "pl"]);
  equal(typeof ctx.addMessages, "function");
  equal(typeof ctx.format, "function");
  equal(typeof ctx.formatToParts, "function");
}

function test_methods_calling(MessageContext) {
  const ctx = new MessageContext(["en-US", "pl"], {
    useIsolating: false
  });
  ctx.addMessages("key = Value");

  const msg = ctx.getMessage("key");
  equal(ctx.format(msg), "Value");
  deepEqual(ctx.formatToParts(msg), ["Value"]);

  ctx.addMessages("key2 = Hello { $name }");

  const msg2 = ctx.getMessage("key2");
  equal(ctx.format(msg2, { name: "Amy" }), "Hello Amy");
  deepEqual(ctx.formatToParts(msg2), ["Hello ", {
    value: "name",
    opts: undefined
  }]);
  ok(true);
}
