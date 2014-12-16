// Basic [[Enumerate]] functionality test
let inner = [];
let handler = {
    enumerate: function(target) {
        assertEq(target, inner);
        assertEq(arguments.length, 1);
        assertEq(this, handler);
        return (function*() { yield 'a'; })();
    }
};

let x;
for (let y in new Proxy(inner, handler)) {
    x = y;
}
assertEq(x, 'a');
