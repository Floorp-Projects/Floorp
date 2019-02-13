// Multiple debuggers get their job queues drained after each hook.
// This covers:
// - onDebuggerStatement
// - onStep
// - onEnterFrame
// - onPop
// - onExceptionUnwind
// - breakpoint handlers
// - uncaughtExceptionHook

const g = newGlobal({ newCompartment: true });
g.parent = this;

var log = '';
let expected_throws = 0;

function setup(global, label) {
  const dbg = new Debugger;
  dbg.gDO = dbg.addDebuggee(global);
  dbg.log = '';

  dbg.onDebuggerStatement = function (frame) {
    // Exercise the promise machinery: resolve a promise and perform a microtask
    // checkpoint. When called from a debugger hook, the debuggee's microtasks
    // should not run.
    function exercise(name) {
      dbg.log += name + ',';
      log += `${label}-${name}-handler\n`;
      Promise.resolve(42).then(v => {
        assertEq(v, 42);
        log += `${label}-${name}-tail\n`;
      });
    }

    exercise('debugger');

    frame.onStep = function () {
      this.onStep = undefined;
      exercise('step');
    };

    dbg.onEnterFrame = function (frame) {
      dbg.onEnterFrame = undefined;
      frame.onPop = function(completion) {
        assertEq(completion.return, 'escutcheon');
        exercise('pop');
      }

      exercise('enter');
    }

    expected_throws++;
    dbg.onExceptionUnwind = function(frame, value) {
      dbg.onExceptionUnwind = undefined;
      assertEq(value, 'myrmidon');
      exercise('exception');
      if (--expected_throws > 0) {
        return undefined;
      } else {
        return { return: 'escutcheon' };
      }
    };

    // Set a breakpoint on entry to g.breakpoint_here.
    const script = dbg.gDO.getOwnPropertyDescriptor('breakpoint_here').value.script;
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

  return dbg;
}

const dbg1 = setup(g, '1');
const dbg2 = setup(g, '2');
const dbg3 = setup(g, '3');

g.eval(`
  function breakpoint_here() {
    throw 'myrmidon';
  }

  parent.log += 'eval-start\\n';

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
  parent.log += 'stuff to step over\\n';
  breakpoint_here();
  parent.log += 'eval-end\\n';
`);

log += 'main-drain\n'
drainJobQueue();
log += 'main-drain-done\n';

const regex = new RegExp(`eval-start
.-debugger-handler
.-uncaught-handler
.-debugger-tail
.-uncaught-tail
.-debugger-handler
.-uncaught-handler
.-debugger-tail
.-uncaught-tail
.-debugger-handler
.-uncaught-handler
.-debugger-tail
.-uncaught-tail
.-step-handler
.-step-tail
.-step-handler
.-step-tail
.-step-handler
.-step-tail
stuff to step over
.-enter-handler
.-enter-tail
.-enter-handler
.-enter-tail
.-enter-handler
.-enter-tail
.-bp-handler
.-bp-tail
.-bp-handler
.-bp-tail
.-bp-handler
.-bp-tail
.-exception-handler
.-exception-tail
.-exception-handler
.-exception-tail
.-exception-handler
.-exception-tail
.-pop-handler
.-pop-tail
.-pop-handler
.-pop-tail
.-pop-handler
.-pop-tail
eval-end
main-drain
eval-reactmain-drain-done
`);

assertEq(!!log.match(regex), true)

assertEq(dbg1.log, 'debugger,uncaught,step,enter,bp,exception,pop,');
assertEq(dbg2.log, 'debugger,uncaught,step,enter,bp,exception,pop,');
assertEq(dbg3.log, 'debugger,uncaught,step,enter,bp,exception,pop,');
