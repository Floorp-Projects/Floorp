var a = [];
var x, i;
for (i = 0; i < HOTLOOP + 10; i++) {
    a[i] = function (b) { this.b = b; };
    if (i != HOTLOOP + 9)
        x = a[i].prototype;
}
for (i = 0; i < HOTLOOP + 10; i++)
    x = new a[i];
assertEq(toString.call(x), "[object Object]");
assertEq(Object.getPrototypeOf(x), a[HOTLOOP + 9].prototype);
