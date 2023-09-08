// |jit-test| --ion-gvn=off
function g() {}
function f() {
    for (var i = 0; i < 100; i++) {
        g(...[]);
    }
}
f();
