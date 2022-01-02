var ToObject = getSelfHostedValue("ToObject");

function f(arr) {
    for (var i = 0; i < arr.length; i++) {
        var v = arr[i];
        try {
            var x = 1;
            ToObject({});
            x = 2;
            ToObject(v);
        } catch (e) {
            assertEq(x, 2);
        }
    }
}

var a = [];
for (var i = 0; i < 50; i++) {
    a.push({}, null);
}
f(a);
