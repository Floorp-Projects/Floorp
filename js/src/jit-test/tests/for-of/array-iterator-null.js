// Array.prototype.iterator applied to undefined or null does not throw (until .next is called).

load(libdir + "asserts.js");
for (var v of [undefined, null]) {
    var it = Array.prototype.iterator.call(v);

    // This will throw because the iterator is trying to get v.length.
    assertThrowsInstanceOf(function () { it.next(); }, TypeError);
}
