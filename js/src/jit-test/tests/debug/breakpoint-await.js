if (typeof disassemble !== "function") {
  quit();
}

const g = newGlobal({ newCompartment: true });
g.eval(`
async function func() {
  await 10;
}
async function * func2() {
  await 10;
}
var func_dis = disassemble(func);
var func2_dis = disassemble(func2);
`);
const d = new Debugger();
const dg = d.addDebuggee(g);
const script = dg.makeDebuggeeValue(g.func).script;
const script2 = dg.makeDebuggeeValue(g.func2).script;

function getOffsets(code) {
  let CanSkipAwait_offset = -1;
  let Await_offset = -1;
  let m;
  for (const line of code.split("\n")) {
    m = line.match(/(\d+):\s+\d+\s+CanSkipAwait\s/);
    if (m) {
      CanSkipAwait_offset = parseInt(m[1], 10);
    }

    m = line.match(/(\d+):\s+\d+\s+Await\s/);
    if (m) {
      Await_offset = parseInt(m[1], 10);
    }
  }
  assertEq(CanSkipAwait_offset !== -1, true);
  assertEq(Await_offset !== -1, true);
  return [CanSkipAwait_offset, Await_offset];
}

let [CanSkipAwait_offset, Await_offset] = getOffsets(g.func_dis);
assertEq(script.getEffectfulOffsets().includes(CanSkipAwait_offset), true);
assertEq(script.getEffectfulOffsets().includes(Await_offset), true);

[CanSkipAwait_offset, Await_offset] = getOffsets(g.func2_dis);
assertEq(script2.getEffectfulOffsets().includes(CanSkipAwait_offset), true);
assertEq(script2.getEffectfulOffsets().includes(Await_offset), true);
