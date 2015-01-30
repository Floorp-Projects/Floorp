if (typeof window === "undefined")
    window = this;

Object.defineProperty(window, "foo", {
    get: function() { return 5; },
    configurable: true
});

for (var i = 0; i < 100; ++i)
    assertEq(window.foo, 5);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
