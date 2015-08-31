// Using an environment that is not a debuggee should throw.

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger;
let gw = dbg.addDebuggee(g);
var log;

function check(env) {
  assertEq(env.inspectable, false);
  assertThrowsInstanceOf(() => env.type, Error);
  assertThrowsInstanceOf(() => env.object, Error);
  assertThrowsInstanceOf(() => env.parent, Error);
  assertThrowsInstanceOf(() => env.callee, Error);

  assertThrowsInstanceOf(() => env.names(), Error);
  assertThrowsInstanceOf(() => env.find('x'), Error);
  assertThrowsInstanceOf(() => env.getVariable('x'), Error);
  assertThrowsInstanceOf(() => env.setVariable('x'), Error);
}

dbg.onDebuggerStatement = function (frame) {
  log += 'd';

  let env = frame.environment;
  dbg.removeDebuggee(g);
  check(env);
}

log = '';
g.eval('let x = 42; debugger;');
assertEq(log, 'd');

dbg.addDebuggee(g);
g.eval('function f() { }');
let env = gw.getOwnPropertyDescriptor('f').value.environment;
assertEq(env.type, 'declarative');
dbg.removeDebuggee(g);
check(env);
