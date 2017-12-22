function assertSyntaxError(code) {
    assertThrowsInstanceOf(function () { Function(code); }, SyntaxError, "Function:" + code);
    assertThrowsInstanceOf(function () { eval(code); }, SyntaxError, "eval:" + code);
    var ieval = eval;
    assertThrowsInstanceOf(function () { ieval(code); }, SyntaxError, "indirect eval:" + code);
}

// AsyncFunction statement
assertSyntaxError(`async function f() 0`);

// AsyncFunction expression
assertSyntaxError(`void async function() 0`);
assertSyntaxError(`void async function f() 0`);


if (typeof reportCompare === "function")
    reportCompare(true, true);
