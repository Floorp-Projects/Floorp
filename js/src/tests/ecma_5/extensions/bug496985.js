var a = function() {
    return function ({x: arguments}) {
        return arguments;
    }
}
var b = eval(uneval(a));

assertEq(a()({x: 1}), 1);
assertEq(b()({x: 1}), 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
