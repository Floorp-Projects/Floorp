const g = newGlobal({ newCompartment: true });
g.eval(`
function* func() {
}
`);
const d = new Debugger();
const dg = d.addDebuggee(g);
const script = dg.makeDebuggeeValue(g.func).script;

// The following test assumes the above `func` function has the following
// bytecode sequences:
//
// 00000:  Generator                       # GENERATOR
// 00001:  SetAliasedVar ".generator"      # GENERATOR
// 00006:  InitialYield 0                  # RVAL GENERATOR RESUMEKIND

// Setting a breakpoint at `SetAliasedVar ".generator"` should be disallow.
let caught = false;
try {
  script.setBreakpoint(1, {});
} catch (e) {
  caught = true;
  assertEq(e.message.includes("not allowed"), true);
}

assertEq(caught, true);

// Setting breakpoints to other opcodes should be allowed.
script.setBreakpoint(0, {});
script.setBreakpoint(6, {});

// Offset 1 shouldn't be exposed.
assertEq(script.getPossibleBreakpoints().some(p => p.offset == 1), false);
assertEq(script.getAllColumnOffsets().some(p => p.offset == 1), false);
assertEq(script.getEffectfulOffsets().includes(1), false);
