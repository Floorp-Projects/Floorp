function debug(msg) {
  dump("[mmMessageChannelChild]" + msg + "\n");
}

/**
 * Preparation Test
 */
let port;
let toString = Object.prototype.toString;

(function prepare() {
  debug("Script loaded.");
  addTestReceiver();
  sendAsyncMessage("mmMessagePort:finishScriptLoad", {}, {});
})();

function ok(condition, message) {
  debug('condition: ' + condition  + ', ' + message + '\n');
  if (!condition) {
    sendAsyncMessage("mmMessagePort:fail", { message: message });
    throw 'failed check: ' + message;
  }
}

function is(a, b, message) {
  ok(a===b, message);
}


/**
 * Testing codes.
 */
function addTestReceiver() {
  addMessageListener("BasicTest:PortCreated", basicTest);
  addMessageListener("CloseTest:PortCreated", closeTest);
  addMessageListener("EmptyTest:PortCreated", emptyTest);
  addMessageListener("NotTransferableTest:PortCreated", notTransferableTest);
}

function basicTest(msg) {
  port = msg.ports[0];
  is(toString.call(port), "[object MessagePort]", "created MessagePort.");

  port.onmessage = (message) => {
    is(message.data, "BasicTest:StartTest", "Replied message is correct.");
    port.postMessage("BasicTest:TestOK");
  };

  sendAsyncMessage("BasicTest:FinishPrepare", { message: "OK" });
}

function closeTest(msg) {
  port = msg.ports[0];
  is(toString.call(port), "[object MessagePort]", "created MessagePort.");

  port.onmessage = (message) => {
    ok(message.data,"CloseTest:StartTest", "Replied message is correct.");
    port.postMessage("CloseTest:TestOK");
  };

  port.close();

  sendAsyncMessage("CloseTest:FinishPrepare", { message: "OK"});
}

function emptyTest(msg) {
  let portSize = msg.ports.length;
  is(portSize, 0, "transfered port size is zero.");

  sendAsyncMessage("EmptyTest:FinishPrepare", { message: "OK"});
}

function notTransferableTest(msg) {
  sendAsyncMessage("NotTransferableTest:FinishPrepare", {message: "OK"});
}
