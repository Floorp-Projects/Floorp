// test against future pic support for symbols

if (typeof Symbol === "function") {
    // assignments to watched objects must not be cached
    var obj = {};
    var x = Symbol.for("x");
    obj[x] = 0;
    var hits = 0;
    obj.watch(x, function (id, oldval, newval) { hits++; return newval; });
    for (var i = 0; i < 10; i++)
        obj[x] = i;
    assertEq(hits, 10);

    // assignments to watched properties via ++ must not be cached
    hits = 0;
    for (var i = 0; i < 10; i++)
        obj[x]++;
    assertEq(hits, 10);

    // adding assignment + watchpoint vs. caching
    hits = 0;
    obj = {};
    obj.watch(x, function (id, oldval, newval) { hits++; return newval; });
    for (var i = 0; i < 10; i++) {
        obj[x] = 1;
        delete obj[x];
    }
    assertEq(hits, 10);
}
