// Binary: cache/js-dbg-64-06445f55f009-linux
// Flags: -m -n -a
//

if (!this.emulatedJSON) {
    emulatedJSON = function () {
        function stringify(value, whitelist) {
            var a, i, v;
            switch (typeof value) {
            case 'string':
                if (!(value.propertyIsEnumerable('length'))) {
                    for (i = 0; i < l; i += 1) {
                        k = whitelist[i];
                        if (typeof k === 'string') {
                            if (i %= 'not visited') {}
                        }
                    }
                }
            }
        }
        return {
            stringify: stringify,
        };
    }();
    var testPairs = [ ['{"five":5}'] ]
    for (var i = 0; i < testPairs.length; i++) {
        var pair = testPairs[i];
        var s = emulatedJSON.stringify(pair[1])
    }
}
