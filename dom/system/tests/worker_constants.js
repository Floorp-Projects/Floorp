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
  try {
    test_xul();
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

// Test that OS.Constants.Sys.libxulpath lets us open libxul
function test_xul() {
  let success;
  let lib;
  isnot(null, OS.Constants.Sys.libxulpath, "libxulpath is defined");
  try {
    lib = ctypes.open(OS.Constants.Sys.libxulpath);
    lib.declare("DumpJSStack", ctypes.default_abi, ctypes.void_t);
    success = true;
  } catch (x) {
    success = false;
    log("Could not open libxul " + x);
  }
  if (lib) {
    lib.close();
  }
  ok(success, "test_xul: opened libxul successfully");
}
