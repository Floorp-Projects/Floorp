// |jit-test| error:all-jobs-completed-successfully
// Test that Debugger.Frame.prototype.environment works on suspended
// async function.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
var resolveTop;
var resolveBlock;
var resolveLoop;
var resolveCatch;

async function f() {
  var promises = {
    top: new Promise(r => { resolveTop = r; }),
    block: new Promise(r => { resolveBlock = r; }),
    loop: new Promise(r => { resolveLoop = r; }),
    catch: new Promise(r => { resolveCatch = r; }),
  };

  var value = 42;
  Promise.resolve().then(resolveTop);
  await promises.top;
  {
    let block = "block";
    Promise.resolve().then(resolveBlock);
    await promises.block;
  }
  for (let loop of ["loop"]) {
    Promise.resolve().then(resolveLoop);
    await promises.loop;
  }
  try {
    throw "err";
  } catch (err) {
    Promise.resolve().then(resolveCatch);
    await promises.catch;
  }
  return value;
}
`);

const waitForOnPop = frame => new Promise(r => {
  assertEq(frame.onPop, undefined);
  frame.onPop = () => {
    frame.onPop = undefined;
    r();
  };
});

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  dbg.onEnterFrame = undefined;
};

(async () => {
  const promise = g.f();

  assertEq(!!frame, true);
  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["arguments", "promises", "value"])
  );
  assertEq(frame.environment.getVariable("value"), 42);

  frame.environment.setVariable("value", 43);

  assertEq(frame.environment.getVariable("value"), 43);

  await waitForOnPop(frame);

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["block"])
  );
  assertEq(frame.environment.getVariable("block"), "block");

  await waitForOnPop(frame);

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["loop"])
  );
  assertEq(frame.environment.getVariable("loop"), "loop");

  await waitForOnPop(frame);

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["err"])
  );
  assertEq(frame.environment.getVariable("err"), "err");

  const result = await promise;

  assertEq(result, 43);

  assertThrowsInstanceOf(() => frame.environment, Error);

  throw "all-jobs-completed-successfully";
})();
