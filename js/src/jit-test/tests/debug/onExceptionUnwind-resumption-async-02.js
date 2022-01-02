// |jit-test| error:all-jobs-completed-successfully
// Verifiy that onExceptionUnwind's force-return queues the promise
// microtask to run in the debuggee's job queue, not the debugger's
// AutoDebuggerJobQueueInterruption.

let g = newGlobal({ newCompartment: true });
g.eval(`
  async function asyncFn(x) {
    await Promise.resolve();
    throw new Error();
  }
  function enterDebuggee(){}
`);
const dbg = new Debugger(g);

(async function() {
  let it = g.asyncFn();

  // Force-return when the exception throws after await resume.
  dbg.onExceptionUnwind = () => {
    return { return: "exit" };
  };

  const result = await it;
  assertEq(result, "exit");
  // If execution here is resumed from the debugger's queue, this call will
  // trigger DebuggeeWouldRun exception.
  g.enterDebuggee();

  throw "all-jobs-completed-successfully";
})();
