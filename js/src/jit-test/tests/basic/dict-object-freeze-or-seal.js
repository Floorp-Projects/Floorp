let sym = Symbol();

let o = {x: 1, y: 2, z: 3, a: 4, b: 5, 12345678: 6, [sym]: 7};
for (let i = 0; i < 100; i++) {
    o["foo" + i] = 1;
}
delete o.x;

Object.seal(o);
assertEq(Object.getOwnPropertyNames(o).length, 105);
assertEq(Object.getOwnPropertySymbols(o).length, 1);

assertEq(Object.isSealed(o), true);
assertEq(Object.isFrozen(o), false);

let desc = Object.getOwnPropertyDescriptor(o, "y");
assertEq(desc.writable, true);
assertEq(desc.configurable, false);

Object.freeze(o);
assertEq(Object.isSealed(o), true);
assertEq(Object.isFrozen(o), true);

desc = Object.getOwnPropertyDescriptor(o, "y");
assertEq(desc.writable, false);
assertEq(desc.configurable, false);
