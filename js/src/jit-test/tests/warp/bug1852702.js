// |jit-test| --fast-warmup
function main() {
    var f = function(x) {
        x.f = f; 
        var f2 = x.f;
        f2[Symbol.toPrimitive] = null;
        try {
            new f2(0);
        } catch (e) {}
        for (var i = 0; i < 100; i++) { } 
    };
    new f(f);
}
gczeal(2);
for (var i = 0; i < 100; i++) {
    main();
}
