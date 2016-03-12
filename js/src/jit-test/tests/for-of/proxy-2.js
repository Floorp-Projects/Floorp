// Basic for-of test with Proxy whose iterator method is a generator.

var arr = ['a', 'b', 'c', 'd'];
var proxy = new Proxy(arr, {
    get(target, property, receiver) {
        if (property === Symbol.iterator) {
            return function* () {
                for (var i = 0; i < arr.length; i++)
                    yield arr[i];
            }
        }

        return Reflect.get(target, property, receiver);
    }
});

for (var i = 0; i < 2; i++)
    assertEq([...proxy].join(","), "a,b,c,d");
