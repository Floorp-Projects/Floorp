// An array iterator for a proxy calls the traps in a predictable order.

load(libdir + "asserts.js");

var s = '';
var it = Array.prototype.iterator.call(Proxy.create({
    get: function (recipient, name) {
        if (name == 'length') {
            s += 'L';
            return 2;
        } else {
            s += name;
            return name;
        }
    }
}));

assertEq(it.next(), "0");
s += ' ';
assertEq(it.next(), "1");
s += ' ';
assertThrowsValue(function () { it.next(); }, StopIteration);
assertEq(s, "L0 L1 L");
