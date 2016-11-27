const Cu = Components.utils;

function run_test() {
    var sandbox = Cu.Sandbox('http://www.example.com');
    var regexp = Cu.evalInSandbox("/test/i", sandbox);
    equal(RegExp.prototype.toString.call(regexp), "/test/i");
    var prototype = Cu.evalInSandbox("RegExp.prototype", sandbox);
    equal(typeof prototype.lastIndex, "undefined");
}
