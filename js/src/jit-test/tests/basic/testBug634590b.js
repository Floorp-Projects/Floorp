this.name = "outer";
var sb = evalcx('');
sb.name = "inner";
sb.parent = this;
function f() { return this.name; }
f.notMuchTodo = '42';
assertEq(evalcx('{\n' +
                '  let f = parent.f;\n' +
                '  let name = "block";\n' +
                '  (function () {\n' +
                '    eval(f.notMuchTodo);\n' + // reify Block
                '    var s = "";\n' +
                '    for (i = 0; i < 10; ++i)\n' +
                '      s += f();\n' +
                '    return s;\n' +
                '  })();\n' +
                '}',
                sb),
	 "outerouterouterouterouterouterouterouterouterouter");
