// @@unscopables checks can call getters.

// The @@unscopables property itself can be a getter.
let hit1 = 0;
let x = "global x";
let env1 = {
    x: "env1.x",
    get [Symbol.unscopables]() {
        hit1++;
        return {x: true};
    }
};
with (env1)
    assertEq(x, "global x");
assertEq(hit1, 1);

// It can throw; the exception is propagated out.
function Fit() {}
with ({x: 0, get [Symbol.unscopables]() { throw new Fit; }})
    assertThrowsInstanceOf(() => x, Fit);

// Individual properties on the @@unscopables object can have getters.
let hit2 = 0;
let env2 = {
    x: "env2.x",
    [Symbol.unscopables]: {
        get x() {
            hit2++;
            return true;
        }
    }
};
with (env2)
    assertEq(x, "global x");
assertEq(hit2, 1);

// And they can throw.
with ({x: 0, [Symbol.unscopables]: {get x() { throw new Fit; }}})
    assertThrowsInstanceOf(() => x, Fit);

reportCompare(0, 0);
