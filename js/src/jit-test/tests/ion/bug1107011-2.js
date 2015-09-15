function foo() {
    var x = 0, y = 0, a = new Float32Array(1);
    function bar() {
        x = y;
        y = a[0];
    }
    for (var i = 0; i < 1000; i++) {
        bar();
    }
}
for (var i=0; i < 50; i++)
    foo();
