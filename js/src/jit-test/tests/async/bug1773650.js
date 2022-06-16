// This approach is based on async/debugger-reject-after-fulfill.js
function searchLastBreakpointBeforeReturn(declCode, callCode) {
  const g = newGlobal({ newCompartment: true });
  const dbg = new Debugger(g);
  g.eval(declCode);

  let offset = 0;
  dbg.onEnterFrame = function(frame) {
    if (frame.callee && frame.callee.name == "f") {
      frame.onStep = () => {
        if (!g.returning) {
          return undefined;
        }

        offset = frame.offset;
        return undefined;
      };
    }
  };
  g.eval(callCode);
  drainJobQueue();
  assertEq(offset != 0, true);
  return offset;
}

let declaration = `
  var returning = false;
  async function f() {
    try {
      throw undefined;
    } catch (exc) {
      try {
        return (returning = true, "expected");
      } catch {}
    }
  }`;
let call = "var p = f();"

let offset = searchLastBreakpointBeforeReturn(declaration, call);

let g = newGlobal({ newCompartment: true });
let dbg = new Debugger(g);
g.eval(declaration);

dbg.onEnterFrame = function(frame) {
  if (frame.callee && frame.callee.name == "f") {
    dbg.onEnterFrame = undefined;
    frame.script.setBreakpoint(offset, {
      hit() {
        return { throw: "unexpected" };
      }
    });
  }
};

try {
  g.eval(call);
} catch {}
