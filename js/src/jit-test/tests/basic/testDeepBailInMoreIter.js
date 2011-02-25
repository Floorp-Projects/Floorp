w = (function() { yield })();
w.next();
for (var i = 0; i < 100; ++i) {
    for (v in w) {}
}
