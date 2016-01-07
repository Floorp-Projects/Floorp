// Basic for-of test with Proxy whose iterator method is a generator.

var arr = ['a', 'b', 'c', 'd'];
var proxy = Proxy.create({
    getPropertyDescriptor: function (name) {
        if (name == 'iterator') {
            return {
                configurable: false,
                enumerable: false,
                writeable: false,
                value:  function () {
                    for (var i = 0; i < arr.length; i++)
                        yield arr[i];
                }
            };
        }

        // Otherwise, inherit the property from arr.
        for (var obj = arr; obj; obj = Object.getPrototypeOf(obj)) {
            var desc = Object.getOwnPropertyDescriptor(obj, name);
            if (desc)
                return desc;
        }
        return undefined;
    }
});

for (var i = 0; i < 2; i++)
    assertEq([...proxy].join(","), "a,b,c,d");
