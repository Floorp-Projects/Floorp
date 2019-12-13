var g1 = newGlobal();
var g2 = newGlobal({sameZoneAs: g1});
function f() {
    var o = Object.create(null);
    for (var p in o) {};
}
g1.eval(f.toString());
g2.eval(f.toString());

for (var i=0; i<10; i++) {
    g1.eval("f()");
    g2.eval("f()");
}
