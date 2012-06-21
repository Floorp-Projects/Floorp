load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    var dbg = new Debugger();
    dbg.addDebuggee(Debugger.Object.prototype);
}, TypeError);