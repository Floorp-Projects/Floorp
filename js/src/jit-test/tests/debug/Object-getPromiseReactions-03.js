// Debugger.Object.prototype.getPromiseReactions reports reaction records
// created with `then` and `catch`.

load(libdir + 'match.js');
load(libdir + 'match-debugger.js');

const { Pattern } = Match;
const { OBJECT_WITH_EXACTLY: EXACT } = Pattern;
function EQ(v) {
  return new DebuggerIdentical(v);
}

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger;
var DOg = dbg.addDebuggee(g);

g.eval(`
    function identity(v) { return v; }
    function thrower(e) { throw e; }
    function fib(n) { if (n <= 1) return 1; else return fib(n-1) + fib(n-2); }
    function triangle(n) { return (n+1) * n / 2; }

    var pResolve, pReject;
    var p = new Promise((resolve, reject) => { pResolve = resolve; pReject = reject });
    var p2 = p.then(identity, thrower);
    var p3 = p.then(fib);
    var p4 = p.catch(triangle);
`);

var [DOidentity, DOthrower, DOfib, DOtriangle, DOp, DOp2, DOp3, DOp4] =
    [g.identity, g.thrower, g.fib, g.triangle, g.p, g.p2, g.p3, g.p4].map(p => DOg.makeDebuggeeValue(p));

Match.Pattern([
  EXACT({ resolve: EQ(DOidentity), reject: EQ(DOthrower), result: EQ(DOp2) }),
  EXACT({ resolve: EQ(DOfib), result: EQ(DOp3) }),
  EXACT({ reject: EQ(DOtriangle), result: EQ(DOp4) })
]).assert(DOp.getPromiseReactions(),
          "promiseReactions doesn't return expected reaction list");

g.pResolve(3);
assertEq(DOp.getPromiseReactions().length, 0);
