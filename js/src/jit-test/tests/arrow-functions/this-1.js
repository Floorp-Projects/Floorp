// 'this' is lexically scoped in arrow functions

var obj = {
    f: function (expected) {
        assertEq(this, expected);
        return a => this;
    }
};

var g = obj.f(obj);
assertEq(g(), obj);

var obj2 = {f: obj.f};
var g2 = obj2.f(obj2);
assertEq(g2(), obj2);
assertEq(g(), obj);

