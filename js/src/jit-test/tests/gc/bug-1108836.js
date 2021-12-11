const root = newGlobal();
var g = newGlobal();
for (var indexI = 0; indexI <= 65535; indexI++) {
    eval("/*var " + String.fromCharCode(indexI) + "xx = 1*/");
}
for (var i = 0; i < 100; ++i) {
    gc();
    gcslice(1000000);
}
