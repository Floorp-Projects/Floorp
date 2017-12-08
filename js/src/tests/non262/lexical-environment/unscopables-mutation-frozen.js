// When env[@@unscopables].x changes, bindings can appear even if env is inextensible.

let x = "global";
let unscopables = {x: true};
let env = Object.create(null);
env[Symbol.unscopables] = unscopables;
env.x = "object";
Object.freeze(env);

for (let i = 0; i < 1004; i++) {
    if (i === 1000)
        unscopables.x = false;
    with (env) {
        assertEq(x, i < 1000 ? "global" : "object");
    }
}

reportCompare(0, 0);
