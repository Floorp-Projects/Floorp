Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  if (!Services.prefs.getBoolPref("javascript.options.asyncstack")) {
    do_print("Async stacks are disabled.");
    return;
  }

  function getAsyncStack() {
    return Components.stack;
  }

  // asyncCause may contain non-ASCII characters.
  let testAsyncCause = "Tes" + String.fromCharCode(355) + "String";

  Components.utils.callFunctionWithAsyncStack(function asyncCallback() {
    let stack = Components.stack;

    Assert.equal(stack.name, "asyncCallback");
    Assert.equal(stack.caller, null);
    Assert.equal(stack.asyncCause, null);

    Assert.equal(stack.asyncCaller.name, "getAsyncStack");
    Assert.equal(stack.asyncCaller.asyncCause, testAsyncCause);
    Assert.equal(stack.asyncCaller.asyncCaller, null);

    Assert.equal(stack.asyncCaller.caller.name, "run_test");
    Assert.equal(stack.asyncCaller.caller.asyncCause, null);
  }, getAsyncStack(), testAsyncCause);
}
