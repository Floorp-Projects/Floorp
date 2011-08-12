function test() {
  3 && x++;
}
assertEq(test.toString(), "function test() {\n    3 && x++;\n}");

function f() {
    a[1, 2]++;
}
assertEq(f.toString(), "function f() {\n    a[1, 2]++;\n}");
