let m = parseModule(`
function* values() {}
var iterator = values();
for (var i=0; i < 10000; ++i) {
    for (var x of iterator) {}
}
`);
moduleLink(m);
moduleEvaluate(m);
