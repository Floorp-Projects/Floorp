// |jit-test| skip-if: !('oomTest' in this)

var lfcode = new Array();
oomTest(() => getBacktrace({}));
