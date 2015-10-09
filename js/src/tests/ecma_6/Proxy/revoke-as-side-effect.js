function createProxy(proxyTarget) {
  var {proxy, revoke} = Proxy.revocable(proxyTarget, new Proxy({}, {
    get(target, propertyKey, receiver) {
      print("trap get:", propertyKey);
      revoke();
    }
  }));
  return proxy;
}

var obj;

// [[GetPrototypeOf]]
assertEq(Object.getPrototypeOf(createProxy({})), Object.prototype);
assertEq(Object.getPrototypeOf(createProxy([])), Array.prototype);

// [[SetPrototypeOf]]
obj = {};
Object.setPrototypeOf(createProxy(obj), Array.prototype);
assertEq(Object.getPrototypeOf(obj), Array.prototype);

// [[IsExtensible]]
assertEq(Object.isExtensible(createProxy({})), true);
assertEq(Object.isExtensible(createProxy(Object.preventExtensions({}))), false);

// [[PreventExtensions]]
obj = {};
Object.preventExtensions(createProxy(obj));
assertEq(Object.isExtensible(obj), false);

// [[GetOwnProperty]]
assertEq(Object.getOwnPropertyDescriptor(createProxy({}), "a"), undefined);
assertEq(Object.getOwnPropertyDescriptor(createProxy({a: 5}), "a").value, 5);

// [[DefineOwnProperty]]
obj = {};
Object.defineProperty(createProxy(obj), "a", {value: 5});
assertEq(obj.a, 5);

// [[HasProperty]]
assertEq("a" in createProxy({}), false);
assertEq("a" in createProxy({a: 5}), true);

// [[Get]]
assertEq(createProxy({}).a, undefined);
assertEq(createProxy({a: 5}).a, 5);

// [[Set]]
assertThrowsInstanceOf(() => createProxy({}).a = 0, TypeError);
assertThrowsInstanceOf(() => createProxy({a: 5}).a = 0, TypeError);

// [[Delete]]
assertEq(delete createProxy({}).a, true);
assertEq(delete createProxy(Object.defineProperty({}, "a", {configurable: false})).a, false);

// [[Enumerate]]
for (var k in createProxy({})) {
    // No properties in object.
    assertEq(true, false);
}
for (var k in createProxy({a: 5})) {
    // Properties in object.
    assertEq(k, "a");
}

// [[OwnPropertyKeys]]
assertEq(Object.getOwnPropertyNames(createProxy({})).length, 0);
assertEq(Object.getOwnPropertyNames(createProxy({a: 5})).length, 1);

// [[Call]]
assertEq(createProxy(function() { return "ok" })(), "ok");

// [[Construct]]
// This throws because after the "construct" trap on the proxy is consulted,
// OrdinaryCreateFromConstructor (called because the |q| function's
// [[ConstructorKind]] is "base" per FunctionAllocate) accesses
// |new.target.prototype| to create the |this| for the construct operation, that
// would be returned if |return obj;| didn't override it.
assertThrowsInstanceOf(() => new (createProxy(function q(){ return obj; })),
                       TypeError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
