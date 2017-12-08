// Create a symbol and a wrapper for it.
var s = Symbol();
var so = Object(s);

// Create a symbol-valued property key using |s|.
var o = {[s]: 0};

// The default Symbol.prototype[@@toPrimitive] will unbox the symbol object as needed.
assertEq(o.propertyIsEnumerable(so), true);
assertEq(o.hasOwnProperty(so), true);

// After redefining Symbol.prototype[@@toPrimitive], any calls to the ToPropertyKey
// abstract operation will no longer unbox the symbol object.
Object.defineProperty(Symbol.prototype, Symbol.toPrimitive, {
    value: function() {
        return "foo";
    }
});

// |o| doesn't have a string-valued property named "foo".
assertEq(o.propertyIsEnumerable(so), false);
assertEq(o.hasOwnProperty(so), false);

o.foo = 123;

// After |o.foo| was added, both calls should return true again.
assertEq(o.propertyIsEnumerable(so), true);
assertEq(o.hasOwnProperty(so), true);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
