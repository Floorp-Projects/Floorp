gczeal(2);

function foo() {
        return /foo/;
}

function test() {
    var obj = {};
    for (var i = 0; i < 50; i++) {
        obj["_" + i] = "J";
    }
}

test();
