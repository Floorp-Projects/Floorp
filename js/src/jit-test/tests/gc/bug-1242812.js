// |jit-test| skip-if: !('oomTest' in this)

var lfcode = new Array();
oomTest(() => { let a = [2147483651]; [-1, 0, 1, 31, 32].sort(); });
