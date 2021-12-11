// |jit-test| --ion-osr=off; error:InternalError
function f() {
    while (true) {
        var r = f();
    }
}
f();
