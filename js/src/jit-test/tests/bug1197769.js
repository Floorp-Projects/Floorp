// |jit-test| error:too much recursion
function test() {
    var a = [""];
    var i = 0;
    for (var e in a) {
        if (i == 10) {
            for (var g in []) {}
        }
        throw test();
    }
}
test();
