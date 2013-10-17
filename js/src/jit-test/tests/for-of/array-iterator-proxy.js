// An array iterator for a proxy calls the traps in a predictable order.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var s = '';
var it = Array.prototype[std_iterator].call(Proxy.create({
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

assertIteratorResult(it.next(), "0", false);
s += ' ';
assertIteratorResult(it.next(), "1", false);
s += ' ';
assertIteratorResult(it.next(), undefined, true);
assertEq(s, "L0 L1 L");
