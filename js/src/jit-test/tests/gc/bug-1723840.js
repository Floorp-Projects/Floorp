gczeal(0);
gcstress = 1;
Object.defineProperty(this, "", {
    value: eval
});
gczeal(11);
gczeal(6, 5);
const a = { b: 7 };
const c = [a, ""];
const d = [c];
new WeakMap(d);

