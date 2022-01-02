function f(x) {
    var y;
    gc();
    ++x.x;
}
f(1);
f.call(2, 3);
