// |jit-test| error:TypeError

Object.prototype.apply = Function.prototype.apply;
function testApplyCallHelper(f) {
    var i = 0;
    i.apply(this,[,,2])
}
testApplyCallHelper()
