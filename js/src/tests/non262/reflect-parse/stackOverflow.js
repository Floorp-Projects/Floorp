// |reftest| skip-if(!xulRuntime.shell)
// Bug 632024: no crashing on stack overflow
try {
    Reflect.parse(Array(3000).join("x + y - ") + "z")
} catch (e) { }

if (typeof reportCompare === 'function')
    reportCompare(true, true);
