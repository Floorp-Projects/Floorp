// 'arguments' is banned in a function with a rest param,
// even nested in an arrow-function parameter default value

load(libdir + "asserts.js");

var mistakes = [
    "(...rest) => arguments",
    "(...rest) => (x=arguments) => 0",
    "function f(...rest) { return (x=arguments) => 0; }",
    "function f(...rest) { return (x=(y=arguments) => 1) => 0; }",
];

for (var s of mistakes)
    assertThrowsInstanceOf(function () { eval(s); }, SyntaxError);
