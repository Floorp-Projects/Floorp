// bug 511575

Function.prototype.X = 42;

function ownProperties() {
    var props = {};
    var r = function () {};

    outer:
    for (var a in r) {
        for (var b in r) {
            if (a == b) {
                continue outer;
            }
        }
        print("SHOULD NOT BE REACHED");
        props[a] = true;
    }
    return props;
}

for (var i = 0; i < 12; ++i) {
    print(i);
    ownProperties();
}
