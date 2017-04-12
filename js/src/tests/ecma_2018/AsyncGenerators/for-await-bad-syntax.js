// |reftest| skip-if(release_or_beta)

var AsyncGenerator = async function*(){}.constructor;

function assertSyntaxError(code) {
    var functionCode = `async function* f() { ${code} }`;
    assertThrowsInstanceOf(() => AsyncGenerator(code), SyntaxError, "AsyncGenerator:" + code);
    assertThrowsInstanceOf(() => eval(functionCode), SyntaxError, "eval:" + functionCode);
    var ieval = eval;
    assertThrowsInstanceOf(() => ieval(functionCode), SyntaxError, "indirect eval:" + functionCode);
}

assertSyntaxError(`for await (;;) ;`);

for (var decl of ["", "var", "let", "const"]) {
    for (var head of ["a", "a = 0", "a, b", "[a]", "[a] = 0", "{a}", "{a} = 0"]) {
        // Ends with C-style for loop syntax.
        assertSyntaxError(`for await (${decl} ${head} ;;) ;`);

        // Ends with for-in loop syntax.
        assertSyntaxError(`for await (${decl} ${head} in null) ;`);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
