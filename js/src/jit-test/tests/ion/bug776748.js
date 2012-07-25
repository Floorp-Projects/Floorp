var eCount = 0;
var funs = [function () {}, function () {}];
function someElement(a) {
    ++eCount;
    var i = (eCount >= 8) ? 1 : 0;
    return a[i]
}
var recursionGuard = 0;
function recursiveThing() {
    someElement(funs);
    if (++recursionGuard % 2) {
        e1();
    }
}
function e1() {
    try {} catch (e) {}
    someElement(funs);
    recursiveThing()
}
recursiveThing()
gc();
recursiveThing()
recursiveThing()
