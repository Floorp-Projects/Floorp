function test(name) {
    eval(`
function ${name}(stdlib, foreign, heap) {
    "use asm";
    var ffi = foreign.t;
    return {};
}
${name}(15, 10);
`);
}

test("as");
test("async");
test("await");
test("each");
test("from");
test("get");
test("let");
test("of");
test("set");
test("static");
test("target");
