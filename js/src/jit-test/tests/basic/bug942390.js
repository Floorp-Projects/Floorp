load(libdir + "immutable-prototype.js");

var count = 0;
if (globalPrototypeChainIsMutable()) {
    Object.defineProperty(__proto__, "__iterator__", {
        get: (function() {
            count++;
        })
    });
} else {
    count = 7;
}

if (globalPrototypeChainIsMutable())
    __proto__ = (function(){});

for (var m = 0; m < 7; ++m) {
    for (var a in 6) {}
}

assertEq(count, 7);
