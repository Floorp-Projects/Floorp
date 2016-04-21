try {
  evaluate(` 
    function runTestCase() $ERROR()
    function $ERROR() {
      throw Error
    }
    Object.defineProperty(this, "x", { value: 0 });
    setJitCompilerOption("ion.warmup.trigger", 0)
  `)
  evaluate(`function f() {} f(x)`)
  runTestCase()
} catch (exc) {}
evaluate(`
  g = newGlobal()
  g.parent = this
  g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
      frame.older
    }
  } + ")()")
  try { $ERROR() } catch(e){}
`)
try {
evaluate(`
  x ^= null;
  if (x = 1)
    $ERROR()
`);
} catch(e) {}
