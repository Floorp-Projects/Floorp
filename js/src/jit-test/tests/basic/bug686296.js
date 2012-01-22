load(libdir + "asserts.js");
var o = Object.preventExtensions(new ArrayBuffer);
assertThrowsInstanceOf(function () { o.__proto__ = {}; }, TypeError);
