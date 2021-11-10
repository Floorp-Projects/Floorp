function f() {
    var i = 0;
    while (i != 3000) {
        if (typeof i !== "number") {
            var x = i || 8;
        }
        var next = i + 1;
        i = arguments;
        i = next;
    }
}
f();
