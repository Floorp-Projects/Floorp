// @@unscopables treats properties found on prototype chains the same as other
// properties.

const x = "global x";
const y = "global y";

// obj[@@unscopables].x works when obj.x is inherited via the prototype chain.
let proto = {x: "object x", y: "object y"};
let env = Object.create(proto);
env[Symbol.unscopables] = {x: true, y: false};
with (env) {
    assertEq(x, "global x");
    assertEq(delete x, false);
    assertEq(y, "object y");
}
assertEq(env.x, "object x");

// @@unscopables works if is inherited via the prototype chain.
env = {
    x: "object",
    [Symbol.unscopables]: {x: true, y: true}
};
for (let i = 0; i < 50; i++)
    env = Object.create(env);
env.y = 1;
with (env) {
    assertEq(x, "global x");
    assertEq(y, "global y");
}

// @@unscopables works if the obj[@@unscopables][id] property is inherited.
env = {
    x: "object",
    [Symbol.unscopables]: Object.create({x: true})
};
with (env)
    assertEq(x, "global x");

reportCompare(0, 0);
