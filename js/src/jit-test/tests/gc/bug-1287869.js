// |jit-test| skip-if: !('gczeal' in this)

gczeal(16);
let a = [];
for (let i = 0; i < 1000; i++)
    a.push({x: i});
gc();
