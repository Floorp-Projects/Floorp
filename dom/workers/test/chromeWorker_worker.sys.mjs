/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// our onmessage handler lives in the module.
import _ from "./chromeWorker_worker_submod.sys.mjs";

if (!("ctypes" in self)) {
  throw "No ctypes!";
}

// Go ahead and verify that the ctypes lazy getter actually works.
if (ctypes.toString() != "[object ctypes]") {
  throw "Bad ctypes object: " + ctypes.toString();
}
