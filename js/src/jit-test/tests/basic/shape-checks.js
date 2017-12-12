enableShapeConsistencyChecks();
var o = {};
for (var i = 0; i < 50; i++) {
    o["x" + i] = i;
}
for (var i = 0; i < 50; i += 2) {
    delete o["x" + i];
}
