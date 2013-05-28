// Debugger.Source.prototype

load(libdir + 'asserts.js');

assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.text.call(42)
}, TypeError);
assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.text.call({})
}, TypeError);
assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.text.call(Debugger.Source.prototype)
}, TypeError);
