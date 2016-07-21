if (!('gczeal' in this))
    quit();

gczeal(16);
let a = [];
for (let i = 0; i < 1000; i++)
    a.push({x: i});
gc();
