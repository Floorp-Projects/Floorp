// |reftest| skip-if(!xulRuntime.shell)

assertThrowsInstanceOf(() => evaluate(`
    try { throw null; } catch (e) { eval("for (var e of []) {}") }
`), SyntaxError);

assertThrowsInstanceOf(new Function(`
    try { throw null; } catch (e) { eval("for (var e of []) {}") }
`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
