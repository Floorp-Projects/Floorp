// When allocating a bound function in JIT code, ensure it ends up with the
// correct proto.
function f() {
    var arr = [function(){}, function(){}];

    arr[0].bind = Function.prototype.bind;
    arr[1].bind = Function.prototype.bind;

    var proto = {};
    Object.setPrototypeOf(arr[1], proto);

    for (var i = 0; i < 2000; i++) {
        var fun = arr[Number(i > 1000)];
        var bound = fun.bind(null);
        assertEq(Object.getPrototypeOf(bound), i > 1000 ? proto : Function.prototype);
    }
}
f();
