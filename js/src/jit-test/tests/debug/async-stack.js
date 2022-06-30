// |jit-test| --async-stacks-capture-debuggee-only

const g = newGlobal({newCompartment: true});

const code = `
var stack = "";

async function Async() {
  await 1;
  stack = new Error().stack;
}

function Sync() {
  Async();
}

Sync();
`;

g.eval(code);
drainJobQueue();
assertEq(g.stack.includes("Sync"), false);

let dbg = new Debugger();
dbg.enableAsyncStack(g);

g.eval(code);
drainJobQueue();
assertEq(g.stack.includes("Sync"), true);

dbg.disableAsyncStack(g);

g.eval(code);
drainJobQueue();
assertEq(g.stack.includes("Sync"), false);
