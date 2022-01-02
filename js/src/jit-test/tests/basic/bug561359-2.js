function f(s) {
    var obj = {m: function () { return a; }};
    eval(s);
    return obj;
}
var obj = f("var a = 'right';");
var a = 'wrong';
assertEq(obj.m(), 'right');
