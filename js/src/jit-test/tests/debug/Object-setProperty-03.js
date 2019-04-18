// tests calling script functions via Debugger.Object.prototype.setProperty
// with different receiver objects.
"use strict";
load(libdir + "/asserts.js");

var global = newGlobal({newCompartment: true});
var dbg = new Debugger();
var globalDO = dbg.addDebuggee(global);
var windowProxyDO = globalDO.makeDebuggeeValue(global);
dbg.onDebuggerStatement = onDebuggerStatement;

global.eval(`
let receiver;
function check(value, thisVal) {
  receiver = thisVal;
  if (value !== "value") throw "Unexpected value";
}
const sloppy = {
  set setter(value) { check(value, this); },
};
const strict = {
  set setter(value) { "use strict"; check(value, this); },
};
debugger;
`);

function onDebuggerStatement(frame) {
    const { environment } = frame;
    const sloppy = environment.getVariable("sloppy");
    const strict = environment.getVariable("strict");
    const receiver = () => environment.getVariable("receiver");
    const value = "value";

    assertEq(sloppy.setProperty("setter", value).return, true);
    assertEq(receiver(), sloppy);
    assertEq(sloppy.setProperty("setter", value, sloppy).return, true);
    assertEq(receiver(), sloppy);
    assertEq(sloppy.setProperty("setter", value, strict).return, true);
    assertEq(receiver(), strict);
    assertEq(sloppy.setProperty("setter", value, 1).return, true);
    assertEq(receiver().class, "Number");
    assertEq(sloppy.setProperty("setter", value, true).return, true);
    assertEq(receiver().class, "Boolean");
    assertEq(sloppy.setProperty("setter", value, null).return, true);
    assertEq(receiver(), windowProxyDO);
    assertEq(sloppy.setProperty("setter", value, undefined).return, true);
    assertEq(receiver(), windowProxyDO);
    assertErrorMessage(() => sloppy.setProperty("setter", value, {}), TypeError,
                       "Debugger: expected Debugger.Object, got Object");

    assertEq(strict.setProperty("setter", value).return, true);
    assertEq(receiver(), strict);
    assertEq(strict.setProperty("setter", value, sloppy).return, true);
    assertEq(receiver(), sloppy);
    assertEq(strict.setProperty("setter", value, strict).return, true);
    assertEq(receiver(), strict);
    assertEq(strict.setProperty("setter", value, 1).return, true);
    assertEq(receiver(), 1);
    assertEq(strict.setProperty("setter", value, true).return, true);
    assertEq(receiver(), true);
    assertEq(strict.setProperty("setter", value, null).return, true);
    assertEq(receiver(), null);
    assertEq(strict.setProperty("setter", value, undefined).return, true);
    assertEq(receiver(), undefined);
    assertErrorMessage(() => strict.setProperty("setter", value, {}), TypeError,
                       "Debugger: expected Debugger.Object, got Object");
};
