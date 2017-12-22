// |reftest| skip-if(!xulRuntime.shell)
// Ensure that exceptions thrown by builder methods propagate.
var thrown = false;
try {
    Reflect.parse("42", { builder: { program: function() { throw "expected" } } });
} catch (e) {
    if (e !== "expected")
        throw e;
    thrown = true;
}
if (!thrown)
    throw new Error("builder exception not propagated");

if (typeof reportCompare === 'function')
    reportCompare(true, true);
