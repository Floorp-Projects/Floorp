// Forward to the target if the trap is not defined
assertEq(Proxy({
    foo: 'bar'
}, {}).foo, 'bar');

assertEq(Proxy({
    foo: 'bar'
}, {})['foo'], 'bar');
