// Basic for-of test with Proxy.

function iter(arr) {
    var i = 0;
    return {
        next: function () {
            if (i < arr.length)
                return arr[i++];
            throw StopIteration;
        }
    };
}

function iterableProxy(arr) {
    return Proxy.create({iterate: function () { return iter(arr); }});
}

var s = '';
var arr = ['a', 'b', 'c', 'd'];
var p = iterableProxy(arr);

// Test the same proxy twice. Its iterate method should be called each time.
for (var i = 0; i < 2; i++) {
    var j = 0;
    for (var x of p)
        assertEq(x, arr[j++]);
    assertEq(j, arr.length);
}
