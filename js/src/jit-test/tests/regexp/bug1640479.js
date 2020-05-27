var failures = 0;
var i = 0;

function foo() {
    var e;
    var r = RegExp("(?<_" + (i++) + "a>)");
    try { e = r.exec("a"); } catch {}
    e = r.exec("a");
    if (e.groups === undefined) failures++;
}

oomTest(foo);
assertEq(failures, 0);
