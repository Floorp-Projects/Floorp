
var F, o;

F = function () {};
F.prototype = new ArrayBuffer(1);
o = new F();
assertEq(o.byteLength, 1); // should be no assertion here

o = {};
o.__proto__ = new Int32Array(1);
assertEq(o.buffer.byteLength, 4); // should be no assertion here

F = function () {};
F.prototype = new Int32Array(1);
o = new F();
try {
    o.slice(0, 1);
    reportFailure("Expected an exception!");
} catch (ex) {
}

reportCompare("ok", "ok", "bug 571014");
