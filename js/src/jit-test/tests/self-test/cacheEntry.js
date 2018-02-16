// These tests are checking that CacheEntry_getBytecode properly set an error
// when there is no bytecode registered.
var caught = 0;
var code = cacheEntry("");
try {
    offThreadDecodeScript(code);
}
catch (e) {
    // offThreadDecodeScript does not work with the --no-thread command line option.
    assertEq(e.message.includes("CacheEntry") || e.message.includes("offThreadDecodeScript"), true);
    caught++;
}

code = cacheEntry("");
try {
    evaluate(code, {loadBytecode: true});
}
catch (e) {
    assertEq(e.message.includes("CacheEntry"), true);
    caught++;
}

assertEq(caught, 2);
