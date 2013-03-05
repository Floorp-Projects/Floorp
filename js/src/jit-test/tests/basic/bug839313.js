// Bug 839313: Verify that 'it' does not leave JS_AddValueRoot roots lying around.
var dbg = new Debugger;
it.customNative = assertEq;
