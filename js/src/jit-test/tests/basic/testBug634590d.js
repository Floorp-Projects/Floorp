this.name = "outer";
var sb = evalcx('');
sb.name = "inner";
sb.parent = this;
this.f = function name(outer) {
    if (outer) {
        return function () {
            return name(false);
        }();
    }
    return this.name;
}
assertEq(evalcx('this.f = parent.f;\n' +
                'var s = "";\n' +
                'for (i = 0; i < 10; ++i)\n' +
                '  s += f(true);\n' +
                's',
                sb),
	 "outerouterouterouterouterouterouterouterouterouter");
