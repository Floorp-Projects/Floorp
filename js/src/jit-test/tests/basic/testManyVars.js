const MANY_VARS = Math.pow(2,17);

var code = "function f1() {\n";
code += "  var x0 = 0";
for (var i = 1; i < MANY_VARS; i++)
    code += ", x" + i + " = " + i;
code += ";\n";
for (var i = 0; i < MANY_VARS; i += 100)
    code += "  assertEq(x" + i + ", " + i + ");\n";
code += "  return x80000;\n";
code += "}\n";
eval(code);
assertEq(f1(), 80000);
