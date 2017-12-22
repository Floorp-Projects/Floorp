function makeProxy(type) {
    return new Proxy({}, { ownKeys() { return [type]; } });
}

for (var type of [123, 12.5, true, false, undefined, null, {}, []]) {
    var proxy = makeProxy(type);
    assertThrowsInstanceOf(() => Object.ownKeys(proxy), TypeError);
    assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy), TypeError);
}

type = Symbol();
proxy = makeProxy(type);
assertEq(Object.getOwnPropertySymbols(proxy)[0], type);

type = "abc";
proxy = makeProxy(type);
assertEq(Object.getOwnPropertyNames(proxy)[0], type);

if (typeof reportCompare === "function")
    reportCompare(true, true);
