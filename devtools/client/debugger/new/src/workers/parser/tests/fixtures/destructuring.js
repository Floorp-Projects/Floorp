const { b, resty } = compute(stuff);
const { first: f, last: l } = obj;

const [a, ...rest] = compute(stuff);
const [x] = ["a"];

for (const [index, element] of arr.entries()) {
  console.log(index, element);
}

const { a: aa = 10, b: bb = 5 } = { a: 3 };
const { temp: [{ foo: foooo }] } = obj;

let { [key]: foo } = { z: "bar" };

let [, prefName] = prefsBlueprint[accessorName];
