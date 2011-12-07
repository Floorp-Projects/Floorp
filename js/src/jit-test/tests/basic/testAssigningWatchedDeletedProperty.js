var o = {};
o.watch("p", function() { });

for (var i = 0; i < 10; i++) {
    o.p = 123;
    delete o.p;
}
