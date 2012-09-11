var g;
function store() {
    return g = "v";
}
function dump() {
    return +store();
}
for (var i = 0; i < 2; i++)
    dump();
