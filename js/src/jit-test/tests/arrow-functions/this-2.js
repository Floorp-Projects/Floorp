// 'this' is lexically scoped in direct eval code in arrow functions

var obj = {
    f: function (s) {
        return a => eval(s);
    }
};

var g = obj.f("this");
assertEq(g(), obj);

var obj2 = {g: g, fail: true};
assertEq(obj2.g(), obj);
