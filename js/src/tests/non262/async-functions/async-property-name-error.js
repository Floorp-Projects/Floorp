function assertSyntaxError(code) {
    assertThrowsInstanceOf(() => { Function(code); }, SyntaxError, "Function:" + code);
    assertThrowsInstanceOf(() => { eval(code); }, SyntaxError, "eval:" + code);
    var ieval = eval;
    assertThrowsInstanceOf(() => { ieval(code); }, SyntaxError, "indirect eval:" + code);
}

assertSyntaxError(`({async async: 0})`);
assertSyntaxError(`({async async})`);
assertSyntaxError(`({async async, })`);
assertSyntaxError(`({async async = 0} = {})`);

for (let decl of ["var", "let", "const"]) {
    assertSyntaxError(`${decl} {async async: a} = {}`);
    assertSyntaxError(`${decl} {async async} = {}`);
    assertSyntaxError(`${decl} {async async, } = {}`);
    assertSyntaxError(`${decl} {async async = 0} = {}`);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
