// Test that Debugger.Frame.prototype.environment works on suspended generators.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
function* f() {
  var value = 42;
  yield;
  {
    let block = "block";
    yield;
  }
  for (let loop of ["loop"]) {
    yield;
  }
  try {
    throw "err";
  } catch (err) {
    yield;
  }
  return value;
}
`);

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  dbg.onEnterFrame = undefined;
};

const it = g.f();

assertEq(!!frame, true);

let result = it.next();

assertEq(result.done, false);
assertEq(result.value, undefined);

assertEq(
  JSON.stringify(frame.environment.names()),
  JSON.stringify(["arguments", "value"])
);

assertEq(frame.environment.getVariable("value"), 42);

frame.environment.setVariable("value", 43);

assertEq(frame.environment.getVariable("value"), 43);

result = it.next();

assertEq(
  JSON.stringify(frame.environment.names()),
  JSON.stringify(["block"])
);
assertEq(frame.environment.getVariable("block"), "block");

result = it.next();

assertEq(
  JSON.stringify(frame.environment.names()),
  JSON.stringify(["loop"])
);
assertEq(frame.environment.getVariable("loop"), "loop");

result = it.next();

assertEq(
  JSON.stringify(frame.environment.names()),
  JSON.stringify(["err"])
);
assertEq(frame.environment.getVariable("err"), "err");

result = it.next();

assertEq(result.done, true);
assertEq(result.value, 43);

assertThrowsInstanceOf(() => frame.environment, Error);
