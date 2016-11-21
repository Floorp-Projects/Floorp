// |jit-test| need-for-each

function g() {
    function f(a) {
        delete a.x;
    }
    for each(let w in [Infinity, String()]) {
        let i = f(w);
    }
}
g();
