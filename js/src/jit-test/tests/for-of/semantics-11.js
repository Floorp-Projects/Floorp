// for-of on a proxy causes a predictable sequence of trap calls.

load(libdir + "iteration.js");

var s = '';

var i = 0;
var next_fn = new Proxy(function() {}, {
    apply() {
        s += "n";
        if (i == 3)
            return { value: undefined, done: true };
        return { value: i++, done: false };
    }
});

var it = new Proxy({}, {
    get(target, property, receiver) {
        assertEq(property, "next");
        s += "N";
        return next_fn;
    }
});

var iterator_fn = new Proxy(function() {}, {
    apply() {
        s += 'i';
        return it;
    }
});

var obj = new Proxy({}, {
    get: function (receiver, name) {
        assertEq(name, Symbol.iterator);
        s += "I";
        return iterator_fn;
    }
});

for (var v of obj)
    s += v;

assertEq(s, 'IiNn0Nn1Nn2Nn');
