// Set breakpoints "everywhere" in a function, then call the function and check that
// the breakpoints were added are at the expected columns, and the breakpoints
// were executed in th expected order.
//
// `code` is a JS Script. The final line should define a function `f` to validate.
// `expectedBpts` is a string of spaces and carets ('^'). Throws if we don't hit
// breakpoints on exactly the columns indicated by the carets.
// `expectedOrdering` is a string of integer indices for the offsets that are
// executed, in the order that then are executed. Test code can also push
// additional items into this string using items.push("!").
function assertOffsetColumns(code, expectedBpts, expectedOrdering = null) {
    if (expectedOrdering === null) {
        // The default ordering simply runs the breakpoints in order.
        expectedOrdering = Array.from(expectedBpts.match(/\^/g), (_, i) => i).join(" ");
    }

    // Define the function `f` in a new global.
    const global = newGlobal({newCompartment: true});

    const lines = code.split(/\r?\n|\r]/g);
    const initCode = lines.slice(0, -1).join("\n");
    const execCode = lines[lines.length - 1];

    // Treat everything but the last line as initialization code.
    global.eval(initCode);

    // Run the test code itself.
    global.eval(execCode);

    // Allow some tests to append to a log that will show up in expected ordering.
    const hits = global.hits = [];
    const bpts = new Set();

    // Set breakpoints everywhere and call the function.
    const dbg = new Debugger;
    let debuggeeFn = dbg.addDebuggee(global).makeDebuggeeValue(global.f);
    if (debuggeeFn.isBoundFunction) {
        debuggeeFn = debuggeeFn.boundTargetFunction;
    }

    const { script } = debuggeeFn;
    for (const offset of script.getAllColumnOffsets()) {
        assertEq(offset.lineNumber, 1);
        assertEq(offset.columnNumber <= execCode.length, true);
        bpts.add(offset.columnNumber);

        script.setBreakpoint(offset.offset, {
            hit(frame) {
                hits.push(offset.columnNumber);
            },
        });
    }
    global.f(3);

    const actualBpts = Array.from(execCode, (_, i) => {
        return bpts.has(i + 1) ? "^" : " ";
    }).join("");

    if (actualBpts.trimEnd() !== expectedBpts.trimEnd()) {
        throw new Error(`Assertion failed:
                     code: ${execCode}
            expected bpts: ${expectedBpts}
              actual bpts: ${actualBpts}\n`);
    }

    const indexLookup = new Map(
        Array.from(bpts).sort().map((col, i) => [col, i]));
    const actualOrdering = hits
        .map(item => typeof item === "number" ? indexLookup.get(item) : item)
        .join(" ");

    if (actualOrdering.trimEnd() !== expectedOrdering.trimEnd()) {
        throw new Error(`Assertion failed:
                     code: ${execCode}
                     bpts: ${expectedBpts}
           expected order: ${expectedOrdering}
             actual order: ${actualOrdering}\n`);
    }
}
