// |jit-test| ion-eager

var ARR = [];
try {
    function f() {
        ARR.push(eval.prototype)
    }
    f()
    function eval()(0)
    f()
} catch (e) {}

if (ARR.length !== 2)
    throw new Error("ERROR 1");
if (typeof(ARR[0]) !== 'undefined')
    throw new Error("ERROR 2");
if (typeof(ARR[1]) !== 'object')
    throw new Error("ERROR 3");
