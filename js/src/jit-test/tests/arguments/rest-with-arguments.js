// 'arguments' is allowed with rest parameters.

var args;

function restWithArgs(a, b, ...rest) {
    return arguments;
}

args = restWithArgs(1, 3, 6, 9);
assertEq(args.length, 4);
assertEq(JSON.stringify(args), '{"0":1,"1":3,"2":6,"3":9}');

args = restWithArgs();
assertEq(args.length, 0);

args = restWithArgs(4, 5);
assertEq(args.length, 2);
assertEq(JSON.stringify(args), '{"0":4,"1":5}');

function restWithArgsEval(a, b, ...rest) {
    return eval("arguments");
}

args = restWithArgsEval(1, 3, 6, 9);
assertEq(args.length, 4);
assertEq(JSON.stringify(args), '{"0":1,"1":3,"2":6,"3":9}');

function g(...rest) {
    h();
}
function h() {
    g.arguments;
}
g();

// eval() is evil, but you can still use it with rest parameters!
function still_use_eval(...rest) {
    eval("x = 4");
}
still_use_eval();
