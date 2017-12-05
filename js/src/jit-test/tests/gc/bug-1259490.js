try { eval("3 ** 4") } catch (e) {
    if (!(e instanceof SyntaxError))
        throw e;
    quit();
}
eval(`

gczeal(8);
for (var k = 0; k < 99; ++k) {
    uneval(-(0 ** (Object | 0 * Object)))
}

`)
