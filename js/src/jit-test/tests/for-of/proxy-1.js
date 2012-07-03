// Basic for-of test with Proxy.

function iterableProxy(arr) {
    return Proxy.create({
        getPropertyDescriptor: function (name) {
            for (var obj = arr; obj; obj = Object.getPrototypeOf(obj)) {
                var desc = Object.getOwnPropertyDescriptor(obj, name);
                if (desc)
                    return desc;
            }
            return undefined;
        }
    });
}

var s = '';
var arr = ['a', 'b', 'c', 'd'];
var p = iterableProxy(arr);

// Test the same proxy twice. Each time through the loop, the proxy handler's
// getPropertyDescriptor method will be called 10 times (once for 'iterator',
// five times for 'length', and once for each of the four elements).
for (var i = 0; i < 2; i++) {
    var j = 0;
    for (var x of p)
        assertEq(x, arr[j++]);
    assertEq(j, arr.length);
}
