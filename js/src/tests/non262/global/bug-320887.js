// `var x` should not call the getter of an existing global property.

var hit = 0;
Object.defineProperty(this, "x", {
    get: function () { return ++hit; },
    configurable: true
});
eval("var x;");
assertEq(hit, 0);

// The declaration should not have redefined the global x, either.
assertEq(x, 1);
assertEq(x, 2);

reportCompare(0, 0);
