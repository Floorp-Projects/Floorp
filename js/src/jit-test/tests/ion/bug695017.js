function f() {
    var n = null;
    return n++;
}

print(f());
assertEq(f(), 0);
