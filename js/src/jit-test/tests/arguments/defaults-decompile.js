function f1(a=1) {}
assertEq(f1.toString(), "function f1(a = 1) {\n}");
function f2(a=1, b=2, c=3) {}
assertEq(f2.toString(), "function f2(a = 1, b = 2, c = 3) {\n}");
function f3(a, b, c=1, d=2) {}
assertEq(f3.toString(), "function f3(a, b, c = 1, d = 2) {\n}");
function f4(a, [b], c=1) {}
assertEq(f4.toString(), "function f4(a, [b], c = 1) {\n}");
function f5(a, b, c=1, ...rest) {}
assertEq(f5.toString(), "function f5(a, b, c = 1, ...rest) {\n}");
function f6(a, [b], c=1, ...rest) {}
assertEq(f6.toString(), "function f6(a, [b], c = 1, ...rest) {\n}");
function f7(a, c=d = 190) {}
assertEq(f7.toString(), "function f7(a, c = d = 190) {\n}");
function f8(a=(b = 8)) {
    function nested() {
        return a + b;
    }
    return nested;
}
assertEq(f8.toString(), "function f8(a = b = 8) {\n\n\
    function nested() {\n\
        return a + b;\n\
    }\n\n\
    return nested;\n\
}");
function f9(a, b, c={complexity : .5, is : 40 + great.prop}, d=[42], ...rest) {}
assertEq(f9.toString(), "function f9(a, b, c = {complexity: 0.5, is: 40 + great.prop}, d = [42], ...rest) {\n}");
function f10(a=12) { return arguments; }
assertEq(f10.toString(), "function f10(a = 12) {\n    return arguments;\n}");
function f11(a=(0, c=1)) {}
assertEq(f11.length, 1);
var g = eval("(" + f11 + ")");
assertEq(g.length, 1);
