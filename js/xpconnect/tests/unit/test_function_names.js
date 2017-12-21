/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;

function callback() {}

let sandbox = Cu.Sandbox(this);
let callbackWrapped = Cu.evalInSandbox("(function wrapped() {})", sandbox);

function run_test() {
  let functions = [
    [{ notify: callback }, "callback[test_function_names.js]:JS"],
    [{ notify: { notify: callback } }, "callback[test_function_names.js]:JS"],
    [callback, "callback[test_function_names.js]:JS"],
    [function() {}, "run_test/functions<[test_function_names.js]:JS"],
    [function foobar() {}, "foobar[test_function_names.js]:JS"],
    [function Δ() {}, "Δ[test_function_names.js]:JS"],
    [{ notify1: callback, notify2: callback }, "nonfunction:JS"],
    [{ notify: 10 }, "nonfunction:JS"],
    [{}, "nonfunction:JS"],
    [{ notify: callbackWrapped }, "wrapped[test_function_names.js]:JS"],
  ];

  // Use the observer service so we can get double-wrapped functions.
  var obs = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);

  function observer(subject, topic, data)
  {
    let named = subject.QueryInterface(Ci.nsINamed);
    Assert.equal(named.name, data);
    dump(`name: ${named.name}\n`);
  }
  obs.addObserver(observer, "test-obs-fun", false);

  for (let [f, requiredName] of functions) {
    obs.notifyObservers(f, "test-obs-fun", requiredName);
  }
}
