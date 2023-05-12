var g = newGlobal({newCompartment: true});
g.evaluate(`RegExp.prototype.exec = {};`);
var wrapper = g.evaluate(`/abc.+def/`);
assertEq(RegExp.prototype.test.call(wrapper, "abc"), false);
assertEq(RegExp.prototype.test.call(wrapper, "abcXdef"), true);
assertEq(RegExp.prototype[Symbol.match].call(wrapper, "abc"), null);
assertEq(RegExp.prototype[Symbol.match].call(wrapper, "abcXdef")[0], "abcXdef");
