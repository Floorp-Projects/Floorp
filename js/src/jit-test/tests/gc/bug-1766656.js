// |jit-test| error: Error

const thisGlobal = this;
const otherGlobalSameCompartment = newGlobal({sameCompartmentAs: thisGlobal});
const globals = [thisGlobal, otherGlobalSameCompartment, undefined];
function testProperties(global, count) {
  let {object: source, transplant} = transplantableObject();
  for (let i9 = 0; i9 < count; i9++) {
    source[(0) + i9] = i9;
  }
  transplant(global);
}
for (let global of globals) {
  for (let count of [0, 10, 30]) {
    testProperties(global, count);
  }
}
