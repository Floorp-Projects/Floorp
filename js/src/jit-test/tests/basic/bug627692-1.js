// |jit-test| need-for-each

var loop1 = '', loop2 = '', actual = '';

var obj = {};
for (var i = 0; i < 10; i++) {
    obj['a' + i] = i;
    loop1 += i;
    loop2 += 'a' + i;
}

Object.defineProperty(obj, 'z', {enumerable: true, get: function () {
            for (var y in obj)
                actual += y;
        }});
(function() {
    for each (var e in obj)
        actual += e;
 })();

assertEq(actual, loop1 + loop2 + "z" + "undefined");
