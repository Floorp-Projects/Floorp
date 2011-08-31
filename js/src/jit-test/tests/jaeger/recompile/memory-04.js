
function foo(x, y) {
    gc();
    var z = x + y;
    print(z);
}

foo(0x7ffffff0, 100);
