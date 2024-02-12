// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

// OOM during off-thread initialization shouldn't leak memory.
eval('oomTest(function(){offThreadCompileToStencil("")})');
