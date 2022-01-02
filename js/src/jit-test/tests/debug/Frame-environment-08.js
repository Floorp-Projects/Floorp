// |jit-test| error:all-jobs-completed-successfully
// Test that Debugger.Frame.prototype.environment works on suspended
// async generators.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
var resolveTop;
var resolveBlock;
var resolveLoop;
var resolveCatch;

async function* f() {
  var promises = {
    top: new Promise(r => { resolveTop = r; }),
    block: new Promise(r => { resolveBlock = r; }),
    loop: new Promise(r => { resolveLoop = r; }),
    catch: new Promise(r => { resolveCatch = r; }),
  };

  var value = 42;
  await promises.top;
  {
    let block = "block";
    await promises.block;
  }
  for (let loop of ["loop"]) {
    await promises.loop;
  }
  try {
    throw "err";
  } catch (err) {
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
  const it = g.f();

  assertEq(!!frame, true);

  let promise = it.next();

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["arguments", "promises", "value"])
  );

  //FIXME assertEq(frame.environment.getVariable("value"), 42);

  frame.environment.setVariable("value", 43);

  g.resolveTop();
  await waitForOnPop(frame);

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["block"])
  );
  assertEq(frame.environment.getVariable("block"), "block");

  g.resolveBlock();
  await waitForOnPop(frame);

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["loop"])
  );
  assertEq(frame.environment.getVariable("loop"), "loop");

  g.resolveLoop();
  await waitForOnPop(frame);

  assertEq(
    JSON.stringify(frame.environment.names()),
    JSON.stringify(["err"])
  );
  assertEq(frame.environment.getVariable("err"), "err");

  g.resolveCatch();
  const result = await promise;

  assertEq(result.done, true);
  //FIXME assertEq(result.value, 43);

  assertThrowsInstanceOf(() => frame.environment, Error);

  throw "all-jobs-completed-successfully";
})();
