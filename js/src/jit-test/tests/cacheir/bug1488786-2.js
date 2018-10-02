setJitCompilerOption('ion.forceinlineCaches', 1);

function protoChange() {
    var o = {0: 0, 1: 0, 0x10000: 0, 0x20000: 0};

    var tests = [1, 0, 0x10000, 0x20000];

    function result_map(key, i) {
        if (i > 5 && key == 0x20000)
            return undefined;
        return 0;
    }

    for (var i = 0; i < 10; i++) {
        for (var key of tests) {
            assertEq(o[key], result_map(key, i));
        }

        if (i == 5) {
            delete o[0x20000];
        }
    }
}

for (var i = 0; i < 10; i++) {
    protoChange();
}