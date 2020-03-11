// |jit-test| error:ReferenceError: can't access lexical declaration
function f() {
    for (let x of [1]) {
        let y = new Date(x);
        `Cannot parse "${x}"`;
        let x;
    }
}
f();