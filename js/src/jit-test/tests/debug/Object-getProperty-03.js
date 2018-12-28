// tests calling script functions via Debugger.Object.prototype.getProperty
// with different receiver objects.
"use strict";
load(libdir + "/asserts.js");

var global = newGlobal();
var dbg = new Debugger();
var globalDO = dbg.addDebuggee(global);
dbg.onDebuggerStatement = onDebuggerStatement;

global.eval(`
const sloppy = {
  get getter() { return this; },
};
const strict = {
  get getter() { "use strict"; return this; },
};
debugger;
`);

function onDebuggerStatement(frame) {
    const { environment } = frame;
    const sloppy = environment.getVariable("sloppy");
    const strict = environment.getVariable("strict");

    assertEq(sloppy.getProperty("getter").return, sloppy);
    assertEq(sloppy.getProperty("getter", sloppy).return, sloppy);
    assertEq(sloppy.getProperty("getter", strict).return, strict);
    assertEq(sloppy.getProperty("getter", 1).return.class, "Number");
    assertEq(sloppy.getProperty("getter", true).return.class, "Boolean");
    assertEq(sloppy.getProperty("getter", null).return, globalDO);
    assertEq(sloppy.getProperty("getter", undefined).return, globalDO);
    assertErrorMessage(() => sloppy.getProperty("getter", {}), TypeError,
                       "Debugger: expected Debugger.Object, got Object");

    assertEq(strict.getProperty("getter").return, strict);
    assertEq(strict.getProperty("getter", sloppy).return, sloppy);
    assertEq(strict.getProperty("getter", strict).return, strict);
    assertEq(strict.getProperty("getter", 1).return, 1);
    assertEq(strict.getProperty("getter", true).return, true);
    assertEq(strict.getProperty("getter", null).return, null);
    assertEq(strict.getProperty("getter", undefined).return, undefined);
    assertErrorMessage(() => strict.getProperty("getter", {}), TypeError,
                       "Debugger: expected Debugger.Object, got Object");
};
