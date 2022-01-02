function foo(date) {
    var F = date.split(" ");
    var D = F[0].split("-");
    var C = F[1].split(":");
    return new Date(D[0], D[1], D[2], C[0], C[1], C[2]);
}
function test() {
    with(this) {};
    for (var i = 0; i < 1200; i++)
        foo("13-5-2015 18:30:" + i);
}
test();
