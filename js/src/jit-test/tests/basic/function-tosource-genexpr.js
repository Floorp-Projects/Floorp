function getgen() {
    gen = getgen.caller;
}
var gen;
(getgen() for (x of [1])).next();
assertEq(gen.toSource(), "function genexp() {\n    [generator expression]\n}");
assertEq(gen.toString(), gen.toSource());
