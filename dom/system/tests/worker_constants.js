/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function log(text) {
  dump("WORKER "+text+"\n");
}

function send(message) {
  self.postMessage(message);
}

self.onmessage = function(msg) {
  self.onmessage = function(msg) {
    log("ignored message "+JSON.stringify(msg.data));
  };
  let { isDebugBuild, umask } = msg.data;
  try {
    test_name();
    test_xul();
    test_debugBuildWorkerThread(isDebugBuild);
    test_umaskWorkerThread(umask);
    test_bits();
  } catch (x) {
    log("Catching error: " + x);
    log("Stack: " + x.stack);
    log("Source: " + x.toSource());
    ok(false, x.toString() + "\n" + x.stack);
  }
  finish();
};

function finish() {
  send({kind: "finish"});
}

function ok(condition, description) {
  send({kind: "ok", condition: condition, description:description});
}
function is(a, b, description) {
  send({kind: "is", a: a, b:b, description:description});
}
function isnot(a, b, description) {
  send({kind: "isnot", a: a, b:b, description:description});
}

// Test that OS.Constants.Sys.Name is defined
function test_name() {
  isnot(null, OS.Constants.Sys.Name, "OS.Constants.Sys.Name is defined");
}

// Test that OS.Constants.Sys.DEBUG is set properly in ChromeWorker thread
function test_debugBuildWorkerThread(isDebugBuild) {
  is(isDebugBuild, !!OS.Constants.Sys.DEBUG, "OS.Constants.Sys.DEBUG is set properly on worker thread");
}

// Test that OS.Constants.Sys.umask is set properly in ChromeWorker thread
function test_umaskWorkerThread(umask) {
  is(umask, OS.Constants.Sys.umask,
     "OS.Constants.Sys.umask is set properly on worker thread: " +
     ("0000"+umask.toString(8)).slice(-4));
}

// Test that OS.Constants.Path.libxul lets us open libxul
function test_xul() {
  let lib;
  isnot(null, OS.Constants.Path.libxul, "libxul is defined");
  try {
    lib = ctypes.open(OS.Constants.Path.libxul);
    lib.declare("DumpJSStack", ctypes.default_abi, ctypes.void_t);
  } catch (x) {
    ok(false, "test_xul: Could not open libxul: " + x);
  }
  if (lib) {
    lib.close();
  }
  ok(true, "test_xul: opened libxul successfully");
}

// Check if the value of OS.Constants.Sys.bits is 32 or 64
function test_bits(){
  is(OS.Constants.Sys.bits, ctypes.int.ptr.size * 8, "OS.Constants.Sys.bits is either 32 or 64");
}
