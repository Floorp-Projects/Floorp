var throwCount = 0;

var objs = [
  // This is just bread and butter trigger-the-megamorphic-path stuff
  {x: 2, a: 1},
  {x: 2, b: 1},
  {x: 2, c: 1},
  {x: 2, d: 1},
  {x: 2, e: 1},
  {x: 2, f: 1},

  // These ensure we hit the megamorphic permissive case
  { get x() { return 2; }, l: 2 },
  { get x() { return 2; }, m: 2 },
  { get x() { return 2; }, n: 2 },
  { get x() { return 2; }, o: 2 },
  { get x() { return 2; }, p: 2 },
  { get x() { return 2; }, q: 2 },
];

function bar(o) {
  return o.x;
}

with({}){}

var count = 0;

for (var i = 0; i < 1000; i++) {
  count += bar(objs[i % objs.length]);
}


try {
  // Throw once during a cache miss
  count += bar({ get x() { throwCount++; throw new Error("foo"); }, z: 2 });
} catch(e) {}

try {
  // Populate the shape so we get a cache hit, and then throw during that cache hit
  count += bar({ get x() { return 2 }, z: 2 });
  count += bar({ get x() { throwCount++; throw new Error("foo"); }, z: 2 });
} catch(e) {}

assertEq(count, 2002);
assertEq(throwCount, 2);
