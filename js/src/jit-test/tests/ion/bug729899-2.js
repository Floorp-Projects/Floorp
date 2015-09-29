load(libdir + "immutable-prototype.js");

function f2() {
    if (globalPrototypeChainIsMutable())
        __proto__ = null;
}

for (var j = 0; j < 50; j++)
    f2();
