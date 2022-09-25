/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function testGlobal(name) {
  const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );

  const sandbox = Cu.Sandbox(systemPrincipal);
  Cu.evalInSandbox("this.__name = '" + name + "'", sandbox);
  return sandbox;
}
