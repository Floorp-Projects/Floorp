// |reftest| skip-if(!xulRuntime.shell)

evaluate(`
    try { throw null; } catch (e) { eval("for (var e of []) {}") }
`);

new Function(`
    try { throw null; } catch (e) { eval("for (var e of []) {}") }
`)();

if (typeof reportCompare === "function")
    reportCompare(true, true);
