// for-in with revoked Proxy
load(libdir + "asserts.js");

let {proxy, revoke} = Proxy.revocable({a: 1}, {});

for (let x in proxy)
    assertEq(x, "a")

revoke();

assertThrowsInstanceOf(function() {
    for (let x in proxy)
        assertEq(true, false);
}, TypeError)
