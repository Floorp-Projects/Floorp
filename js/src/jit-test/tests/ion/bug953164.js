function test(a) {
    var total = 0;
    for (var i=0; i<100; i++) {

        var j = 1;
        var b = a.a
        if (b) {
            j += b.test;
        }
        total += j;
    }
    print(total)
}

var a1 = {"a": {"test":1}};
var a2 = {"a": undefined};
test(a1)
test(a2)
test(a1)
test(a2)
