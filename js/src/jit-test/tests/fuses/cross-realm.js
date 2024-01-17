// |jit-test|

function f(x) {
    let [a, b, c] = x;
    return a + b + c;
}

function intact(name) {
    let state = getFuseState();
    if (!(name in state)) {
        throw "No such fuse " + name;
    }
    return state[name].intact
}

let didIt = false;
([])[Symbol.iterator]().__proto__['return'] = () => { didIt = true; return { done: true, value: undefined } };
assertEq(intact("ArrayIteratorPrototypeHasNoReturnProperty"), false);

assertEq(f([1, 2, 3, 0]), 6);
assertEq(didIt, true);

didIt = false;
g = newGlobal();
g.evaluate(f.toString());
// Passing in an array from this realm should mean that the return is triggered.
g.long = [1, 2, 3, 0];
g.evaluate("assertEq(f(long),6)")
g.evaluate(intact.toString());
// ensure fuse isn't popped inside g.
g.evaluate(`assertEq(intact("ArrayIteratorPrototypeHasNoReturnProperty"), true)`)
assertEq(didIt, true);

didIt = false;
g = newGlobal();
g.evaluate(f.toString());
// Passing in an array from this realm should mean that the return is triggered.
g.long = [1, 2, 3, 0];

// Warm up this global's f.
g.evaluate(`
for (let i = 0; i < 100; i++) {
    assertEq(f([1, 2, 3, 0]), 6);
}
`);

assertEq(didIt, false);
g.evaluate("assertEq(f(long), 6)");
assertEq(didIt, true);

delete Array.prototype[Symbol.iterator]
let success = false;
try { f([1, 2, 3, 4]); success = true } catch (e) { }
assertEq(success, false);

try { g.evaluate("assertEq(f(long), 6)"); success = true } catch (e) { }
assertEq(success, false);
