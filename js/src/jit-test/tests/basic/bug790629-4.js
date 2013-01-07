// 'this' in a generator-expression non-strict function produces the expected
// object.

Number.prototype.iters = function () {
    return [(this for (x of [0])),
            (this for (y of [0]))];
};

var [a, b] = (3).iters();
var three = a.next();
assertEq(Object.prototype.toString.call(three), '[object Number]');
assertEq(+three, 3);
assertEq(b.next(), three);
