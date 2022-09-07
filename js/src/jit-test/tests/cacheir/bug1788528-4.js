// |jit-test| error:ReferenceError: can't access lexical declaration
function f(i) {
    if (i === 19) {
        g();
    }
    let val = 0;
    function g() {
        eval("");
        val;
    }
    g();
}
for (var i = 0; i < 20; i++) {
    f(i);
}
