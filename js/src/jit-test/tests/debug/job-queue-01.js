// Debuggee promise reaction jobs should not run from debugger callbacks.
// This covers:
// - onDebuggerStatement
// - onStep
// - onEnterFrame
// - onPop
// - onExceptionUnwind
// - breakpoint handlers
// - uncaughtExceptionHook

var g = newGlobal({ newCompartment: true });
g.parent = this;
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);
var log = '';

// Exercise the promise machinery: resolve a promise and drain the job queue (or
// in HTML terms, perform a microtask checkpoint). When called from a debugger
// hook, the debuggee's microtasks should not run.
function exercise(name) {
  log += `${name}-handler`;
  Promise.resolve(42).then(v => {
    assertEq(v, 42);
    log += `${name}-react`;
  });
  log += `(`;
  drainJobQueue();
  log += `)`;

  // This should be run by the implicit microtask checkpoint after each Debugger
  // hook call.
  Promise.resolve(42).then(v => {
    assertEq(v, 42);
    log += `(${name}-tail)`;
  });
}

dbg.onDebuggerStatement = function (frame) {
  exercise('debugger');

  frame.onStep = function () {
    this.onStep = undefined;
    exercise('step');
  };

  dbg.onEnterFrame = function (frame) {
    dbg.onEnterFrame = undefined;
    frame.onPop = function(completion) {
      assertEq(completion.return, 'recompense');
      exercise('pop');
    }

    exercise('enter');
  }

  dbg.onExceptionUnwind = function(frame, value) {
    dbg.onExceptionUnwind = undefined;
    assertEq(value, 'recidivism');
    exercise('exception');
    return { return: 'recompense' };
  };

  // Set a breakpoint on entry to g.breakpoint_here.
  const script = gDO.getOwnPropertyDescriptor('breakpoint_here').value.script;
  const handler = {
    hit(frame) {
      script.clearAllBreakpoints();
      exercise('bp');
    }
  };
  script.setBreakpoint(0, handler);

  dbg.uncaughtExceptionHook = function (ex) {
    assertEq(ex, 'turncoat');
    exercise('uncaught');
  };

  // Throw an uncaught exception from the Debugger handler. This should reach
  // uncaughtExceptionHook, but shouldn't affect the debuggee.
  throw 'turncoat';
};

g.eval(`
  function breakpoint_here() {
    throw 'recidivism';
  }

  parent.log += 'eval(';

  // DebuggeeWouldRun detection may prevent this callback from running at all if
  // bug 1145201 is present. SpiderMonkey will try to run the promise reaction
  // job from the Debugger hook's microtask checkpoint, triggering
  // DebuggeeWouldRun. This is a little difficult to observe, since the callback
  // never even begins execution. But it should cause the 'then' promise to be
  // rejected, which the shell will report (if the assertEq(log, ...) doesn't
  // kill the test first).

  Promise.resolve(84).then(function(v) {
    assertEq(v, 84);
    parent.log += 'eval-react';
  });
  debugger;
  parent.log += '...';
  breakpoint_here();
  parent.log += ')';
`);

log += 'main-drain('
drainJobQueue();
log += ')';

assertEq(log, `eval(\
debugger-handler(debugger-react)\
uncaught-handler((debugger-tail)uncaught-react)(uncaught-tail)\
step-handler(step-react)(step-tail)\
...\
enter-handler(enter-react)(enter-tail)\
bp-handler(bp-react)(bp-tail)\
exception-handler(exception-react)(exception-tail)\
pop-handler(pop-react)(pop-tail)\
)\
main-drain(eval-react)`);
