var g = newGlobal();
g.eval(`
Array.from(function*() {
    yield 1;
    relazifyFunctions();
    yield 1;
}());
`);
