// for-of on a proxy causes a predictable sequence of trap calls.

var s = '';

var i = 0;
var next_fn = Proxy.createFunction({}, function () {
    s += "n";
    if (i == 3)
        throw StopIteration;
    return i++;
});

var it = Proxy.create({
    get: function (receiver, name) {
        if (name == 'toSource') {
            s += '?';
            return function () 'it';
        }
        assertEq(name, "next");
        s += "N";
        return next_fn;
    }
});

var iterator_fn = Proxy.createFunction({}, function () {
    s += 'i';
    return it;
});

var obj = Proxy.create({
    get: function (receiver, name) {
        assertEq(name, "iterator");
        s += "I";
        return iterator_fn;
    }
});

for (var v of obj)
    s += v;

assertEq(s, 'IiNn0Nn1Nn2Nn');
