// The .environment of a function Debugger.Object is an Environment object.

load(libdir + 'nightly-only.js');

var g = newGlobal()
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

function check(expr) {
  print("checking " + uneval(expr));
  let completion = gDO.executeInGlobal(expr);
  if (completion.throw)
    throw completion.throw.unsafeDereference();
  assertEq(completion.return.environment instanceof Debugger.Environment, true);
}

g.eval('function j(a) { }');

check('j');
check('(() => { })');
check('(function f() { })');
check('(function* g() { })');
check('(async function m() { })');
nightlyOnly(g.SyntaxError, () => {
  check('(async function* n() { })');
});
