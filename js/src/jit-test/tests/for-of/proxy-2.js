// Basic for-of test with Proxy whose iterate method is a generator.

var arr = ['a', 'b', 'c', 'd'];
var proxy = Proxy.create({
    iterate: function () {
        for (var i = 0; i < arr.length; i++)
            yield arr[i];
    }
});

for (var i = 0; i < 2; i++)
    assertEq([v for (v of proxy)].join(","), "a,b,c,d");
