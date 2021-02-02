// |jit-test| error: TypeError: get length method
function f(o) {
  return o.length;
}
let objects = [
  {},
  {length: 0},
  [],
  {x: 0, length: 0},
  {x: 0, y: 0, length: 0},
  {x: 0, y: 0, z: 0, length: 0},
  new Uint32Array(),
  Object.create(new Uint8Array()),
];
objects.forEach(f);
