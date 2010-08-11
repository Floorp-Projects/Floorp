var jetpack = null;

load("handle_tests.js");
function createHandle() {
  return jetpack.createHandle();
}

function run_test() {
  jetpack = createJetpack({
    skipRegisterError: true,
    scriptFile: do_get_file("impl.js")
  });
  run_handle_tests();

  var circ1 = {},
      circ2 = {},
      circ3 = {},
      ok = false;
  ((circ1.next = circ2).next = circ3).next = circ1;
  try {
    jetpack.sendMessage("ignored", circ3, circ1);
    ok = true;
  } catch (x) {
    do_check_false(x);
  }
  do_check_true(ok);

  var echoHandle = jetpack.createHandle();
  echoHandle.payload = { weight: 10 };
  jetpack.registerReceiver("echo",
                           function(msgName, data, handle) {
                             do_check_eq(arguments.length, 3);
                             do_check_eq(msgName, "echo");
                             do_check_eq(data, "echo this");
                             do_check_true(handle.isValid);
                             do_check_eq(handle, echoHandle);
                             do_check_eq(handle.payload.weight, 10);
                             do_test_finished();
                           });

  jetpack.registerReceiver("callback",
                           function(msgName, data) {
                             do_check_eq(msgName, "callback");
                             return "called back: " + data;
                           });

  var callbackHandle = echoHandle.createHandle();
  jetpack.registerReceiver("sendback",
                           function(msgName, data, handle) {
                             do_check_eq(msgName, "sendback");
                             do_check_eq(data, "called back: call me back");
                             do_check_eq(handle, callbackHandle);
                             do_test_finished();
                           });

  var obj;
  jetpack.registerReceiver("recvHandle",
                           function(msgName, data, handle) {
                             handle.mark = obj = {};
                             jetpack.sendMessage("kthx", data + data, handle.createHandle());
                           });
  jetpack.registerReceiver("recvHandleAgain",
                           function(msgName, data, handle) {
                             do_check_eq(data, "okokokok");
                             do_check_eq(handle.mark, obj);
                             do_test_finished();
                           });
  var obj1 = {
    id: Math.random() + ""
  }, obj2 = {
    id: Math.random() + "",
    obj: obj1
  };
  jetpack.registerReceiver("echo2",
                           function(msgName, a, b) {
                             do_check_neq(obj1, a);
                             do_check_neq(obj2, b);
                             do_check_eq(obj1.id, a.id);
                             do_check_eq(obj2.id, b.id);
                             do_check_eq(obj1.id, obj2.obj.id);
                             do_test_finished();
                           });

  jetpack.registerReceiver("multireturn", function() { return obj1 });
  jetpack.registerReceiver("multireturn", function() { return circ1 });
  jetpack.registerReceiver("multireturn", function() { return obj2 });
  jetpack.registerReceiver("multireturn check",
                           function(msgName, rval1, rval2, rval3) {
                             do_check_eq(rval1.id, obj1.id);
                             do_check_eq(rval2.next.next.next, rval2);
                             do_check_eq(rval3.id, obj2.id);
                             do_check_eq(rval3.obj.id, obj1.id);
                             do_test_finished();
                           });

  var testarray = [1, 1, 2, 3, 5, 8, 13];
  jetpack.registerReceiver("testarray",
                           function(msgName, reversed) {
                             for (var i = 0; i < testarray.length; ++i)
                               do_check_eq(testarray[i],
                                           reversed[reversed.length - i - 1]);
                             do_test_finished();
                           });

  var undefined;
  jetpack.registerReceiver("test primitive types",
                           function(msgName,
                                    void_val, null_val,
                                    bool_true, bool_false,
                                    one, two, nine99,
                                    one_quarter,
                                    oyez_str)
                           {
                             do_check_true(void_val === undefined);
                             do_check_true(null_val === null);
                             do_check_true(bool_true === true);
                             do_check_true(bool_false === false);
                             do_check_eq(one, 1);
                             do_check_eq(two, 2);
                             do_check_eq(nine99, 999);
                             do_check_eq(one_quarter, 0.25);
                             do_check_eq(oyez_str, "oyez");

                             do_test_finished();
                           });

  var drop = {
    nested: {
      method: function() { return this.value },
      value: 42
    }
  };
  jetpack.registerReceiver("drop methods",
                           function(msgName, echoed) {
                             do_check_true(!echoed.nested.method);
                             do_check_eq(echoed.nested.value, 42);
                             do_test_finished();
                           });

  var coped = "did not cope";
  jetpack.registerReceiver("exception coping",
                           function(msgName) { throw coped = "did cope" });
  jetpack.registerReceiver("exception coping",
                           function(msgName) {
                             do_check_eq(coped, "did cope");
                             do_test_finished();
                           });

  var calls = "";
  function countCalls() { calls += "." }
  jetpack.registerReceiver("duplicate receivers", countCalls);
  jetpack.registerReceiver("duplicate receivers", countCalls);
  jetpack.registerReceiver("duplicate receivers",
                           function() { do_check_eq(calls, ".") });
  jetpack.registerReceiver("duplicate receivers", countCalls);
  jetpack.registerReceiver("duplicate receivers",
                           function() {
                             do_check_eq(calls, ".");
                             jetpack.unregisterReceivers("duplicate receivers");
                           });
  jetpack.registerReceiver("duplicate receivers",
                           function() { do_test_finished() });

  jetpack.registerReceiver("test result", function(name, c, msg) {
    dump("TEST-INFO | test_jetpack.js | remote check '" + msg + "' result: " + c + "\n");
    do_check_true(c);
  });
  jetpack.registerReceiver("sandbox done", do_test_finished);

  jetpack.registerReceiver("core:exception",
			   function(msgName, e) {
			       do_check_true(/throwing on request/.test(e.message));
			       do_test_finished();
			   });

  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();

  jetpack.sendMessage("echo", "echo this", echoHandle);
  jetpack.sendMessage("callback", "call me back", callbackHandle);
  jetpack.sendMessage("gimmeHandle");
  jetpack.sendMessage("echo2", obj1, obj2);
  jetpack.sendMessage("multireturn begin");
  jetpack.sendMessage("testarray", testarray);
  jetpack.sendMessage("test primitive types",
                      undefined, null, true, false, 1, 2, 999, 1/4, "oyez");
  jetpack.sendMessage("drop methods", drop);
  jetpack.sendMessage("exception coping");

  jetpack.sendMessage("duplicate receivers");

  jetpack.sendMessage("test sandbox");
  jetpack.sendMessage("throw");
}
