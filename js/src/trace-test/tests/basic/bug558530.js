// There's no assertEq() here;  the test is just that it doesn't crash.
(function () {
    new function () {}
}());
[function () {}]
gc()
for (z = 0; z < 6; ++z) {
    x = []
}
for (w in [0]) {
    x._ = w
}
gc()

