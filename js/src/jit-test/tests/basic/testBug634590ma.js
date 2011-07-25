// |jit-test| mjitalways;

this.name = "outer";
var sb = evalcx('');
sb.name = "inner";
sb.parent = this;
function f() { return this.name; }
assertEq(evalcx('this.f = parent.f;\n' +
                'var s = "";\n' +
                'for (i = 0; i < 10; ++i)\n' +
                '  s += f();\n' +
                's',
                sb),
	 "outerouterouterouterouterouterouterouterouterouter");
