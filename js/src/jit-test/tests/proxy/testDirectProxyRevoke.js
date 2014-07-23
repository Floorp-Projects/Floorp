load(libdir + "asserts.js");

// Test for various properties demanded of Proxy.revocable
var holder = Proxy.revocable({}, {});

// The returned object must inherit from Object.prototype
assertEq(Object.getPrototypeOf(holder), Object.prototype);

assertDeepEq(Object.getOwnPropertyNames(holder), [ 'proxy', 'revoke' ]);

// The revocation function must inherit from Function.prototype
assertEq(Object.getPrototypeOf(holder.revoke), Function.prototype);

// Proxy.revoke should return unique objects from the same opcode call.
var proxyLog = []
var revokerLog = []
var holderLog = []

function addUnique(l, v)
{
    assertEq(l.indexOf(v), -1);
    l.push(v);
}

for (let i = 0; i < 5; i++) {
    holder = Proxy.revocable({}, {});
    addUnique(holderLog, holder);
    addUnique(revokerLog, holder.revoke);
    addUnique(proxyLog, holder.proxy);
}

// The provided revoke function should revoke only the 1 proxy
var p = proxyLog.pop();
var r = revokerLog.pop();

// Works before, but not after. This is mostly a token. There are
// testDirectProxy* tests to check each trap revokes properly.
p.foo;
assertEq(r(), undefined, "Revoke trap must return undefined");
assertThrowsInstanceOf(() => p.foo, TypeError);
assertEq(r(), undefined, "Revoke trap must return undefined");

// None of this should throw, since these proxies are unrevoked.
for (let i = 0; i < proxyLog.length; i++)
    proxyLog[i].foo;
