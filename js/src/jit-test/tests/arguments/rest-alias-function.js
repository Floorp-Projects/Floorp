function no_defaults(a, ...rest) {
    return rest;
    function rest() { return a; }
}
assertEq(typeof no_defaults(), "function");
function with_defaults(a=42, ...rest) {
    return rest;
    function rest() { return a; }
}
assertEq(typeof with_defaults(), "function");
