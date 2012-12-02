function test_storage_builtins(aStorage) {
    test(function() {
        aStorage.clear();
        assert_equals(aStorage.length, 0, "aStorage.length");

        var builtins = ["key", "getItem", "setItem", "removeItem", "clear"];
        var origBuiltins = builtins.map(function(b) { return Storage.prototype[b]; });
        assert_array_equals(builtins.map(function(b) { return aStorage[b]; }), origBuiltins, "a");
        builtins.forEach(function(b) { aStorage[b] = b; });
        assert_array_equals(builtins.map(function(b) { return aStorage[b]; }), origBuiltins, "b");
        assert_array_equals(builtins.map(function(b) { return aStorage.getItem(b); }), builtins, "c");

        assert_equals(aStorage.length, builtins.length, "aStorage.length");
    });
}
