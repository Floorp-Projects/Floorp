var o = {};
for(var i=0; i<5; i++) {
    o.p = 2;
    o.watch("p", function() { });
    o.p = 2;
    delete o.p;
}
