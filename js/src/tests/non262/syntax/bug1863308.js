try {
    eval("for (let case of ['foo', 'bar']) {}")
}
catch(e) {
    assertEq(e instanceof SyntaxError, true)
    assertEq(e.message, "unexpected token: keyword 'case'");
}

try {
    eval("for (let debugger of ['foo', 'bar']) {}")
}
catch(e) {
    assertEq(e instanceof SyntaxError, true)
    assertEq(e.message, "unexpected token: keyword 'debugger'");
}

try {
    eval("for (let case in ['foo', 'bar']) {}")
}
catch(e) {
    assertEq(e instanceof SyntaxError, true)
    assertEq(e.message, "unexpected token: keyword 'case'");
}

try {
    eval("for (let debugger in ['foo', 'bar']) {}")
}
catch(e) {
    assertEq(e instanceof SyntaxError, true)
    assertEq(e.message, "unexpected token: keyword 'debugger'");
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);

