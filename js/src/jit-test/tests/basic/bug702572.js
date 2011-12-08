eval("function f() { function g() {} return g; }");
assertEq(f().prototype !== f().prototype, true);

function f1() {
    function g1() {
        function h1() { return h1; }
    }
    return g1;
}
assertEq(f1().prototype !== f1().prototype, true);
