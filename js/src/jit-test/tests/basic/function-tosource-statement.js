function f1(a, b) {
    return a + b;
}
assertEq(f1.toString(), "function f1(a, b) {\n    return a + b;\n}");
assertEq(f1.toSource(), f1.toString());
function f2(a, /* ))))pernicious comment */ b,
            c, // another comment((
            d) {}
assertEq(f2.toString(), "function f2(a, /* ))))pernicious comment */ b,\n            c, // another comment((\n            d) {}");
assertEq(decompileBody(f2), "");
function f3() { }
assertEq(f3.toString(), "function f3() { }");
assertEq(decompileBody(f3), " ");
