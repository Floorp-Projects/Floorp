g = (function (...rest) { return rest; });
assertEq(g.toString(), "function (...rest) {\n    return rest;\n}");
