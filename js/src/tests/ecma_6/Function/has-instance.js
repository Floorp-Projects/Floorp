// It is possible to override Function.prototype[@@hasInstance].
let passed = false;
let obj = { foo: true };
let C = function(){};

Object.defineProperty(C, Symbol.hasInstance, {
  value: function(inst) { passed = inst.foo; return false; }
});

assertEq(obj instanceof C, false);
assertEq(passed, true);

{
    let obj = {
        [Symbol.hasInstance](v) { return true; },
    };
    let whatevs = {};
    assertEq(whatevs instanceof obj, true);
}

{

    function zzzz() {};
    let xxxx = new zzzz();
    assertEq(xxxx instanceof zzzz, true);
    assertEq(zzzz[Symbol.hasInstance](xxxx), true);

}

// Non-callable objects should return false.
const nonCallables = [
    1,
    undefined,
    null,
    "nope",
]

for (let nonCallable of nonCallables) {
    assertEq(nonCallable instanceof Function, false);
    assertEq(nonCallable instanceof Object, false);
}

// It should be possible to call the Symbol.hasInstance method directly.
assertEq(Function.prototype[Symbol.hasInstance].call(Function, () => 1), true);
assertEq(Function.prototype[Symbol.hasInstance].call(Function, Object), true);
assertEq(Function.prototype[Symbol.hasInstance].call(Function, null), false);
assertEq(Function.prototype[Symbol.hasInstance].call(Function, Array), true);
assertEq(Function.prototype[Symbol.hasInstance].call(Object, Array), true);
assertEq(Function.prototype[Symbol.hasInstance].call(Array, Function), false);
assertEq(Function.prototype[Symbol.hasInstance].call(({}), Function), false);
assertEq(Function.prototype[Symbol.hasInstance].call(), false)
assertEq(Function.prototype[Symbol.hasInstance].call(({})), false)

// Primitives should throw
assertThrowsInstanceOf(() => {
    Function.prototype[Symbol.hasInstance].call(1, Function);
}, TypeError);

// Non-callables should throw for the default method
assertThrowsInstanceOf(() => {
    function foo() {};
    let obj = {};
    foo instanceof obj;
}, TypeError);

// However non-callables do not throw for overridden methods
for (let nonCallable of [1, undefined, null, {}]) {
    let o = {[Symbol.hasInstance](v) { return true; }}
    assertEq(nonCallable instanceof o, true);
}

// Ensure that bound functions are unwrapped properly
let bindme = {x: function() {}};
let instance = new bindme.x();
let xOuter = bindme.x;
let bound = xOuter.bind(bindme);
let doubleBound = bound.bind(bindme);
let tripleBound = bound.bind(doubleBound);
assertEq(Function.prototype[Symbol.hasInstance].call(bound, instance), true);
assertEq(Function.prototype[Symbol.hasInstance].call(doubleBound, instance), true);
assertEq(Function.prototype[Symbol.hasInstance].call(tripleBound, instance), true);

// Function.prototype[Symbol.hasInstance] is not configurable
let desc = Object.getOwnPropertyDescriptor(Function.prototype, Symbol.hasInstance)
assertEq(desc.configurable, false)

if (typeof reportCompare === "function")
    reportCompare(true, true);
