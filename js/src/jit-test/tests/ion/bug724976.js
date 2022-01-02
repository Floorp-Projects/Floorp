function g(a) {}
for(var i = 0; i < 10000; i++) {
    g(0 || 1);
}
