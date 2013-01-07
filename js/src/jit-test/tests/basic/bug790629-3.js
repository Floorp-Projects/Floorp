// 'this' in escaping generator-expression in a method
// is the same as 'this' in the enclosing method
// even if the method does not otherwise use 'this'.

var obj = {
    f: function () {
        return (this for (x of [0]));
    }
};

assertEq(obj.f().next(), obj);
