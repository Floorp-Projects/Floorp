// |jit-test| error:all-jobs-completed-successfully

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
    async function f() {
        capture();
        await Promise.resolve(3);
        return "ok";
    }
`);


const promise = g.f();
promise.then(value => {
  assertEq(value, "ok");

  Pattern([
    EXACT({ return: new DebuggerObjectPattern("Promise"), await:true }),
    EXACT({ return: new DebuggerObjectPattern("Promise") }),
  ]).assert(log);

  throw "all-jobs-completed-successfully";
});
