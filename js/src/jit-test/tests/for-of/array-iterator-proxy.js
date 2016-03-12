// An array iterator for a proxy calls the traps in a predictable order.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var s = '';

var handler = {
    get: function (recipient, name) {
        if (name == 'length') {
            s += 'L';
            return 2;
        } else {
            s += name;
            return name;
        }
    }
};

var it = Array.prototype[Symbol.iterator].call(new Proxy([0, 1], handler));

assertIteratorNext(it, "0");
s += ' ';
assertIteratorNext(it, "1");
s += ' ';
assertIteratorDone(it, undefined);
assertEq(s, "L0 L1 L");

s = '';
var ki = Array.prototype.keys.call(new Proxy([0, 1], handler));

assertIteratorNext(ki, 0);
s += ' ';
assertIteratorNext(ki, 1);
s += ' ';
assertIteratorDone(ki, undefined);
assertEq(s, "L L L");

s = '';
var ei = Array.prototype.entries.call(new Proxy([0, 1], handler));

assertIteratorNext(ei, [0, "0"]);
s += ' ';
assertIteratorNext(ei, [1, "1"]);
s += ' ';
assertIteratorDone(ei, undefined);
assertEq(s, "L0 L1 L");
