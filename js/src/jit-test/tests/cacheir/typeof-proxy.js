function test() {
    var funs = [function() {}, new Proxy(function() {}, {}), wrapWithProto(function() {}, null)];
    var objects = [{}, new Proxy({}, {}), wrapWithProto({}, null), new Proxy(objectEmulatingUndefined(), {})];
    var undefs = [objectEmulatingUndefined(), wrapWithProto(objectEmulatingUndefined(), null)];

    for (var fun of funs) {
        assertEq(typeof fun, "function")
    }

    for (var obj of objects) {
        assertEq(typeof obj, "object");
    }

    for (var undef of undefs) {
        assertEq(typeof undef, "undefined");
    }
}

test();
