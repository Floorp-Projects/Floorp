// Iterator.prototype.next throws if applied to a value that isn't an iterator.

load(libdir + "asserts.js");
for (var v of [null, undefined, false, 0, "ponies", {}, [], this])
    assertThrowsInstanceOf(function () { Iterator.prototype.next.call(v); }, TypeError);
