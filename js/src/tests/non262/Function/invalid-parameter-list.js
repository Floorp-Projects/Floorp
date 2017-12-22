// This constructor behaves like `Function` without checking
// if the parameter list end is at the expected position.
// We use this to make sure that the tests we use are otherwise
// syntactically correct.
function DumpFunction(...args) {
    let code = "function anonymous(";
    code += args.slice(0, -1).join(", ");
    code += ") {\n";
    code += args[args.length -1];
    code += "\n}";
    eval(code);
}

const tests = [
    ["/*", "*/) {"],
    ["//", ") {"],
    ["a = `", "` ) {"],
    [") { var x = function (", "} "],
    ["x = function (", "}) {"]
];

for (const test of tests) {
    DumpFunction(...test);
    assertThrowsInstanceOf(() => new Function(...test), SyntaxError);
}

reportCompare(0, 0, 'ok');
