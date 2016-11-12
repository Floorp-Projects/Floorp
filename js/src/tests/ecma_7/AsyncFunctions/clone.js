// |reftest| skip-if(!xulRuntime.shell) -- needs clone

// Async function cannot be cloned.
assertThrowsInstanceOf(() => clone(async function f() {}), TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
