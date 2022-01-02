var f = Function("a", "b", "return a + b;");
assertEq(f.toString(), "function anonymous(a,b\n) {\nreturn a + b;\n}");
if (Function.prototype.toSource) {
    assertEq(f.toSource(), "(function anonymous(a,b\n) {\nreturn a + b;\n})");
}
assertEq(decompileFunction(f), f.toString());
f = Function("a", "...rest", "return rest[42] + b;");
assertEq(f.toString(), "function anonymous(a,...rest\n) {\nreturn rest[42] + b;\n}");
if (Function.prototype.toSource) {
    assertEq(f.toSource(), "(function anonymous(a,...rest\n) {\nreturn rest[42] + b;\n})")
}
assertEq(decompileFunction(f), f.toString());
f = Function("");
assertEq(f.toString(), "function anonymous(\n) {\n\n}");
f = Function("", "(abc)");
assertEq(f.toString(), "function anonymous(\n) {\n(abc)\n}");
