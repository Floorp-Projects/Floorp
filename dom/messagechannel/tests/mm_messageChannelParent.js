let port;
let mm;

function info(message) {
  return window.arguments[0].info(message);
}

function ok(condition, message) {
  return window.arguments[0].ok(condition, message);
}

function is(v1, v2, message) {
  return window.arguments[0].is(v1, v2, message);
}

function todo_is(v1, v2, message) {
  return window.arguments[0].todo_is(v1, v2, message);
}

function cleanUp() {
  window.arguments[0].setTimeout(function() {
    this.done();
  }, 0);
  window.close();
}

function debug(msg) {
  dump("[mmMessageChannelParent]" + msg + "\n");
}

let tests = [basic_test, close_test, empty_transferable, not_transferable];

// Test Routine
function run_tests() {
  let test = tests.shift();
  if (test === undefined) {
    cleanUp();
    return;
  }

  test(function() {
    setTimeout(run_tests, 0);
  });
}

// Basic communication test.
function basic_test(finish) {
  ok(mm, "basic_test");

  let finishPrepare = msg => {
    is(msg.data.message, "OK", "");
    ok(port, "");
    port.onmessage = message => {
      is(message.data, "BasicTest:TestOK", "");
      finish();
    };
    port.postMessage("BasicTest:StartTest");
    mm.removeMessageListener("BasicTest:FinishPrepare", finishPrepare);
  };

  let channel = new MessageChannel();
  port = channel.port2;
  mm.addMessageListener("BasicTest:FinishPrepare", finishPrepare);
  mm.sendAsyncMessage("BasicTest:PortCreated", {}, {}, [channel.port1]);
}

// Communicate with closed port.
function close_test(finish) {
  ok(mm, "close_test");

  let finishPrepare = msg => {
    is(msg.data.message, "OK", "");
    ok(port, "");

    port.onmessage = message => {
      ok(false, "Port is alive.");
      finish();
    };

    port.postMessage("CloseTest:StartTest");
    mm.removeMessageListener("CloseTest:FinishPrepare", finishPrepare);
    finish();
  };

  let channel = new MessageChannel();
  port = channel.port2;
  mm.addMessageListener("CloseTest:FinishPrepare", finishPrepare);
  mm.sendAsyncMessage("CloseTest:PortCreated", {}, {}, [channel.port1]);
}

// Empty transferable object
function empty_transferable(finish) {
  ok(mm, "empty_transferable");

  let finishPrepare = msg => {
    ok(true, "Same basic test.");
    mm.removeMessageListener("EmptyTest:FinishPrepare", finishPrepare);
    finish();
  };

  mm.addMessageListener("EmptyTest:FinishPrepare", finishPrepare);
  mm.sendAsyncMessage("EmptyTest:PortCreated", {}, {}, []);
}

// Not transferable object.
function not_transferable(finish) {
  ok(mm, "not_transferable");

  let finishPrepare = msg => {
    ok(true, "Same basic test.");
    finish();
  };

  mm.addMessageListener("NotTransferableTest:FinishPrepare", finishPrepare);
  mm.sendAsyncMessage("NotTransferableTest:PortCreated", {}, {}, [""]);
}

/*
 * Test preparation
 */
function finishLoad(msg) {
  run_tests();
}

function prepare_test() {
  debug("start run_tests()");
  var node = document.getElementById("messagechannel_remote");
  mm = node.messageManager; //Services.ppmm.getChildAt(1);
  ok(mm, "created MessageManager.");

  mm.addMessageListener("mmMessagePort:finishScriptLoad", finishLoad);
  mm.addMessageListener("mmMessagePort:fail", failed_test);
  //mm.loadProcessScript("chrome://mochitests/content/chrome/dom/messagechannel/tests/mm_messageChannel.js", true);
  mm.loadFrameScript(
    "chrome://mochitests/content/chrome/dom/messagechannel/tests/mm_messageChannel.js",
    true
  );
  ok(true, "Loaded");
}

function failed_test() {
  debug("failed test in child process");
  ok(false, "");
}
