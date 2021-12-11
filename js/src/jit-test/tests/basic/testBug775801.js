function f() {
    var x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15,x16,x17,x18,x19,x20;
    var b = {a:'ponies'};
    eval('');
    return function(e) { return b[e] }
}

assertEq("aaa".replace(/a/g, f()), "poniesponiesponies");
