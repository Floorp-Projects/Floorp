// |jit-test| skip-if: !hasFunction.oomAtAllocation

function code(n) {
    return `
        // Trigger top-level execution with an OOM in the middle.
        oomAtAllocation(${n});
        try { getSelfHostedValue("numberFormatCache") } catch (e) { }
        resetOOMFailure();

        // Read current value of "dateTimeFormatCache".
        var initVal = getSelfHostedValue("dateTimeFormatCache");
        assertEq(typeof initVal, "object");

        // Retrigger top-level execution by reading a later value in the file.
        // Then compare that "dateTimeFormatCache" was not clobbered.
        getSelfHostedValue("collatorCache");
        assertEq(initVal, getSelfHostedValue("dateTimeFormatCache"));
        `;
}

// We cannot use `oomTest` here because of divergence issues from things like
// `RegisterShapeCache` absorbing OOMs.
for (var i = 1; i < 300; ++i) {
    evaluate(code(i), { global: newGlobal() });
}
