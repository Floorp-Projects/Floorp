// String.prototype.iterator is generic.

load(libdir + "asserts.js");
load(libdir + "iteration.js");
load(libdir + "string.js");

function test(obj) {
    var it = String.prototype[std_iterator].call(obj);
    var s = String(obj);
    for (var i = 0, length = s.length; i < length;) {
        var r = s[i++];
        if (isHighSurrogate(r) && i < length && isLowSurrogate(s[i])) {
            r += s[i++];
        }
        assertIteratorNext(it, r);
    }
    assertIteratorDone(it, undefined);
}

test({toString: () => ""});
test({toString: () => "xyz"});
test({toString: () => "\ud808\udf45"});
test({valueOf: () => ""});
test({valueOf: () => "xyz"});
test({valueOf: () => "\ud808\udf45"});
