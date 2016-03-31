/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test ThreadSafeDevToolsUtils.isSet

function run_test() {
  const { isSet } = DevToolsUtils;

  equal(isSet(new Set()), true);
  equal(isSet(new Map()), false);
  equal(isSet({}), false);
  equal(isSet("I swear I'm a Set"), false);
  equal(isSet(5), false);

  const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
        .createInstance(Ci.nsIPrincipal);
  const sandbox = new Cu.Sandbox(systemPrincipal);

  equal(isSet(Cu.evalInSandbox("new Set()", sandbox)), true);
  equal(isSet(Cu.evalInSandbox("new Map()", sandbox)), false);
  equal(isSet(Cu.evalInSandbox("({})", sandbox)), false);
  equal(isSet(Cu.evalInSandbox("'I swear I\\'m a Set'", sandbox)), false);
  equal(isSet(Cu.evalInSandbox("5", sandbox)), false);
}
