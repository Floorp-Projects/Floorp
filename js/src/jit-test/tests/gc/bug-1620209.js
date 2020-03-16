// |jit-test| skip-if: helperThreadCount() === 0
verifyprebarriers();
offThreadCompileScript('');
var dbg = new Debugger();
var objects = dbg.findObjects();
