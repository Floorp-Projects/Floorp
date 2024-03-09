// |jit-test| --fast-warmup
var i = 0;
function a() {
    if (i++ > 50) {
        return;
    }
    function b() {
        new a("abcdefghijklm");
    }
    [new b];
}
gczeal(4);
a();
