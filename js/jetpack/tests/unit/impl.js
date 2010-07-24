function echo() {
  sendMessage.apply(this, arguments);
}

registerReceiver("echo", echo);

registerReceiver("callback",
                 function(msgName, data, handle) {
                   sendMessage("sendback",
                               callMessage("callback", data)[0],
                               handle);
                 });

registerReceiver("gimmeHandle",
                 function(msgName) {
                   sendMessage("recvHandle", "ok", createHandle());
                 });

registerReceiver("kthx",
                 function(msgName, data, child) {
                   sendMessage("recvHandleAgain", data + data, child.parent);
                 });

registerReceiver("echo2", echo);

registerReceiver("multireturn begin",
                 function() {
                   var results = callMessage("multireturn");
                   sendMessage.apply(null, ["multireturn check"].concat(results));
                 });

registerReceiver("testarray",
                 function(msgName, array) {
                   sendMessage("testarray", array.reverse());
                 });

registerReceiver("test primitive types", echo);

registerReceiver("drop methods", echo);

registerReceiver("exception coping", echo);

registerReceiver("duplicate receivers", echo);

function ok(c, msg)
{
  sendMessage("test result", c, msg);
}

registerReceiver("test sandbox", function() {
  var addon = createSandbox();
  ok(typeof(addon) == "object", "typeof(addon)");
  ok("Date" in addon, "addon.Date exists");
  ok(addon.Date !== Date, "Date objects are different");

  var fn = "var x; var c = 3; function doit() { x = 12; return 4; }";
  evalInSandbox(addon, fn);

  ok(addon.x === undefined, "x is undefined");
  ok(addon.c == 3, "c is 3");
  ok(addon.doit() == 4, "doit called successfully");
  ok(addon.x == 12, "x is now 12");

  var fn2 = "let function barbar{}";
  try {
    evalInSandbox(addon, fn2);
    ok(false, "bad syntax should throw");
  }
  catch(e) {
    ok(true, "bad syntax should throw");
  }

  var fn3 = "throw new Error('just kidding')";
  try {
    evalInSandbox(addon, fn3);
    ok(false, "thrown error should be caught");
  }
  catch(e) {
    ok(true, "thrown error should be caught");
  }

  sendMessage("sandbox done");
});

registerReceiver("throw", function(msgName) {
  throw new Error("throwing on request");
});
