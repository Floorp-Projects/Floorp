// |jit-test| exitstatus: 3
if (typeof oomAtAllocation !== 'function')
    quit(3);

var lfcode = new Array();
function k(a, f_arg, b, c) {
    for (var i = 0; i < 5; ++i) {
        f_arg(i + a);
    }
}
function t() {
    var x = 2;
    k(50, function(i) {
        x = i;
    }, 100, 200);
    oomAtAllocation(101);
}

t();
t();

quit(3);
