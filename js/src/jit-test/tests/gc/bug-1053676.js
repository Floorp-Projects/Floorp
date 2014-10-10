// |jit-test| ion-eager;
if (typeof Symbol !== "function") quit(0);

var x
(function() {
    x
}());
verifyprebarriers();
x = x * 0
x = Symbol();
gc();
evalcx("x=1", this);
