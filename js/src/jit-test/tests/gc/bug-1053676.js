// |jit-test| --ion-eager

var x
(function() {
    x
}());
verifyprebarriers();
x = x * 0
x = Symbol();
gc();
evalcx("x=1", this);
