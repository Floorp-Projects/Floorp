// Array.prototype.reverse does a strict assignment to this.length even if the
// caller is nonstrict. Bug 886087.

load(libdir + "asserts.js");

// obj[1] is read-only
var obj = {0: "zero", length: 2, reverse: [].reverse};
Object.defineProperty(obj, "1", {configurable: true, value: "one", writable: false});
assertThrowsInstanceOf(() => obj.reverse(), TypeError);

// obj[1] has only a getter
Object.defineProperty(obj, "1", {configurable: true, get: () => "one"});
assertThrowsInstanceOf(() => obj.reverse(), TypeError);
