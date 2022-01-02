let g = newGlobal({newCompartment: true});
let d = new Debugger(g);

d.onDebuggerStatement = function (frame) {
  frame.environment;
};

g.evaluate(`
  function * foo() {
    // Force CallObject + LexicalEnvironmentObject
    let x;
    let y = () => x;

    // Force DebuggerEnvironment
    debugger;

    // Force suspend and frame snapshot
    yield;

    // Popping this frame will trigger a second snapshot
  }
`)

let x = g.foo();
x.next();
x.next();
