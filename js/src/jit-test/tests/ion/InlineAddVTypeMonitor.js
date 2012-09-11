// make sure AddV instruction is monitoring the output type when 'f' is inlined
// into the loop of 'g'
var x = {};
var y = [];

function f(i) {
    return x + y;
}

function g(m) {
    var i;
    for (i = 0; i < m; i++) {
        f(i);
    }
}

g(101);
