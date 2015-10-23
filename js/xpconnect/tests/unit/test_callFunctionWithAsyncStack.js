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

    do_check_eq(stack.name, "asyncCallback");
    do_check_eq(stack.caller, null);
    do_check_eq(stack.asyncCause, null);

    do_check_eq(stack.asyncCaller.name, "getAsyncStack");
    do_check_eq(stack.asyncCaller.asyncCause, testAsyncCause);
    do_check_eq(stack.asyncCaller.asyncCaller, null);

    do_check_eq(stack.asyncCaller.caller.name, "run_test");
    do_check_eq(stack.asyncCaller.caller.asyncCause, null);
  }, getAsyncStack(), testAsyncCause);
}
