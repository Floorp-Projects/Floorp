// defineProperty throws if a getter or setter is neither undefined nor callable.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

for (let v of [null, false, 'bad', 0, 2.76, {}]) {
    assertThrowsInstanceOf(function () {
        gw.defineProperty("p", {configurable: true, get: v});
    }, TypeError);
    assertThrowsInstanceOf(function () {
        gw.defineProperty("p", {configurable: true, set: v});
    }, TypeError);
}
