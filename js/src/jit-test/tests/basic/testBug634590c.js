this.name = "outer";
var sb = evalcx('');
sb.name = "inner";
sb.parent = this;
function f() { return this.name; }
f.notMuchTodo = '42';
assertEq(evalcx('(function () {\n' +
                '  arguments = null;\n' + // force heavyweight
                '  var f = parent.f;\n' +
                '  var name = "call";\n' +
                '  return (function () {\n' +
                '    eval(f.notMuchTodo);\n' + // reify Call, make f() compile to JSOP_CALLNAME
                '    var s = "";\n' +
                '    for (i = 0; i < 10; ++i)\n' +
                '      s += f();\n' +
                '    return s;\n' +
                '  })();\n' +
                '})()',
                sb),
	 "outerouterouterouterouterouterouterouterouterouter");
