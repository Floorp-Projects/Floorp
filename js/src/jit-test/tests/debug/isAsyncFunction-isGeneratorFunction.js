// Debugger.Script.prototype.isAsyncFunction, Debugger.Object.prototype.isAsyncFunction,
// Debugger.Script.prototype.isGeneratorFunction, Debugger.Object.prototype.isGeneratorFunction

var g = newGlobal();
var dbg = new Debugger();
var gDO = dbg.addDebuggee(g);
g.non_debuggee = function non_debuggee () {}

function checkExpr(expr, { isAsync, isGenerator })
{
  print("Evaluating: " + uneval(expr));
  let fn = gDO.executeInGlobal(expr).return;

  assertEq(fn.isAsyncFunction, isAsync);
  assertEq(fn.isGeneratorFunction, isGenerator);

  // The Debugger.Object and its Debugger.Script should always agree.
  if (fn.script) {
    assertEq(fn.isAsyncFunction, fn.script.isAsyncFunction);
    assertEq(fn.isGeneratorFunction, fn.script.isGeneratorFunction);
  }
}

checkExpr('({})', { isAsync: undefined, isGenerator: undefined });
checkExpr('non_debuggee', { isAsync: undefined, isGenerator: undefined });
checkExpr('(function(){})', { isAsync: false, isGenerator: false });
checkExpr('(function*(){})', { isAsync: false, isGenerator: true });
checkExpr('(async function snerf(){})', { isAsync: true, isGenerator: false });
checkExpr('(async function* omlu(){})', { isAsync: true, isGenerator: true });

checkExpr('new Function("1+2")', { isAsync: false, isGenerator: false });
checkExpr('Object.getPrototypeOf(function*(){}).constructor("1+2")',
          { isAsync: false, isGenerator: true });
checkExpr('Object.getPrototypeOf(async function(){}).constructor("1+2")',
          { isAsync: true, isGenerator: false });
checkExpr('Object.getPrototypeOf(async function*(){}).constructor("1+2")',
          { isAsync: true, isGenerator: true });

// Check eval scripts.
function checkFrame(expr, type)
{
  var log = '';
  dbg.onDebuggerStatement = function(frame) {
    log += 'd';
    assertEq(frame.type, type);
    assertEq(frame.script.isAsyncFunction, false);
    assertEq(frame.script.isGeneratorFunction, false);
  }
  gDO.executeInGlobal(expr);
  assertEq(log, 'd');
}

checkFrame('debugger;', 'global');
checkFrame('eval("debugger;")', 'eval');
