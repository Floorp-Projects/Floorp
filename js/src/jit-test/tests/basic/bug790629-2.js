// 'this' in escaping generator-expression in a method
// is the same as 'this' in the enclosing method.

var obj = {
    f: function () {
        assertEq(this, obj);
        return (this for (x of [0]));
    }
};

assertEq(obj.f().next(), obj);
