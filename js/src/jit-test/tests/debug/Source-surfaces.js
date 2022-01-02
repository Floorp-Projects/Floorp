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

assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.element.call(42)
}, TypeError);
assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.element.call({})
}, TypeError);
assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.element.call(Debugger.Source.prototype)
}, TypeError);

assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.elementAttributeName.call(42)
}, TypeError);
assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.elementAttributeName.call({})
}, TypeError);
assertThrowsInstanceOf(function () {
    Debugger.Source.prototype.elementAttributeName.call(Debugger.Source.prototype)
}, TypeError);
