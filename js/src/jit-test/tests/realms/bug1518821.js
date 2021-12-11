load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var f = g.Function("fn", "fn()");
f(function() {
    nukeAllCCWs();
    assertErrorMessage(() => { arguments.callee.caller = null; }, TypeError,
                       "can't access dead object");
    assertErrorMessage(() => arguments.callee.caller, TypeError,
                       "can't access dead object");
});
