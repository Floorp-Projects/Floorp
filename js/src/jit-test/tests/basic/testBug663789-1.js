function f() {
    a = arguments;
    return a[0] - a[1];
}

[1,2,3,4].sort(f);
