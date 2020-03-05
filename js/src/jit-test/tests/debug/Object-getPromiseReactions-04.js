// Debugger.Object.prototype.getPromiseReactions reports reaction records
// created by `await` expressions in async functions.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger;
const DOg = dbg.addDebuggee(g);

g.eval(`
    var pResolve, pReject;
    var p0 = new Promise((resolve, reject) => { pResolve = resolve; pReject = reject });

    // In this case, promiseReactions encounters a Debugger.Frame we had already
    // associated with the generator, when we hit the debugger statement.
    async function f1() { debugger; await p0; }

    // In this case, promiseReactions must construct the Debugger.Frame itself,
    // since it is the first to encounter the generator.
    async function f2() { await p0; debugger; }
`);

let DFf1, DFf2;
dbg.onDebuggerStatement = (frame) => {
  DFf1 = frame;
  dbg.onDebuggerStatement = (frame) => {
    DFf2 = frame;
    dbg.onDebuggerStatement = () => { throw "Shouldn't fire twice"; };
  };
};

g.eval(`var p1 = f1();`);
assertEq(DFf1.callee.name, "f1");

g.eval(`var p2 = f2();`);
assertEq(DFf2, undefined);

const [DOp0, DOp1, DOp2] =
      [g.p0, g.p1, g.p2].map(p => DOg.makeDebuggeeValue(p));

const reactions = DOp0.getPromiseReactions();
assertEq(reactions.length, 2);
assertEq(reactions[0], DFf1);
assertEq(true, reactions[1] instanceof Debugger.Frame);

// Let f2 run until it hits its debugger statement.
g.pResolve(42);
drainJobQueue();
assertEq(DFf2.terminated, true);
assertEq(reactions[1], DFf2);
