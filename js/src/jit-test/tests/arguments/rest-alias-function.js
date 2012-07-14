function no_defaults(a, ...rest) {
    return rest;
    function rest() { return a; }
}
assertEq(typeof no_defaults(), "function");
assertEq(no_defaults.toString(), "function no_defaults(a, ...rest) {\n    return rest;\n\n    function rest() {\n        return a;\n    }\n\n}");
function with_defaults(a=42, ...rest) {
    return rest;
    function rest() { return a; }
}
assertEq(typeof with_defaults(), "function");
assertEq(with_defaults.toString(), "function with_defaults(a = 42, ...rest) {\n    return rest;\n\n    function rest() {\n        return a;\n    }\n\n}");
