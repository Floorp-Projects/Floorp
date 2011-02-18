this.name = "outer";
var sb = evalcx('');
sb.name = "inner";
sb.parent = this;
function f() { return this.name; }
assertEq(evalcx('this.f = parent.f; var s = ""; for (i = 0; i < 10; ++i) s += f(); s', sb),
	 "innerinnerinnerinnerinnerinnerinnerinnerinnerinner");
