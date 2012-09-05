// Forward to the target if the trap is not defined
assertEq(new Proxy({
    foo: 'bar'
}, {}).foo, 'bar');

assertEq(new Proxy({
    foo: 'bar'
}, {})['foo'], 'bar');
