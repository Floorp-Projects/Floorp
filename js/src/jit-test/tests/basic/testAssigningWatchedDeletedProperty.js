var o = {};
o.watch("p", function() { });

for (var i = 0; i < HOTLOOP + 2; i++) {
    o.p = 123;
    delete o.p;
}
