load(libdir + "asserts.js");
load(libdir + 'match.js');
load(libdir + 'match-debugger.js');
const { Pattern } = Match;
const { OBJECT_WITH_EXACTLY: EXACT } = Pattern;

let g = newGlobal({newCompartment: true});
let dbg = Debugger(g);
const log = [];
g.capture = function () {
  dbg.getNewestFrame().onPop = completion => {
    log.push(completion);
  };
};

g.eval(`
    function* f() {
        capture();
        yield 3;
        return "ok";
    }
`);

assertDeepEq([... g.f()], [3]);
Pattern([
  EXACT({ return: new DebuggerObjectPattern("Object", { value: 3, done: false }), yield: true }),
  EXACT({ return: new DebuggerObjectPattern("Object", { value: "ok", done: true }) }),
]).assert(log);
