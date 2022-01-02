const thisGlobal = this;
const otherGlobalSameCompartment = newGlobal({sameCompartmentAs: thisGlobal});
const otherGlobalNewCompartment = newGlobal({newCompartment: true});

const globals = [thisGlobal, otherGlobalSameCompartment, otherGlobalNewCompartment];

function testProperties(global, count) {
  let {object: source, transplant} = transplantableObject();

  // Create a bunch properties on |source|, which force allocation of dynamic
  // slots.
  for (let i = 0; i < count; i++) {
    source["foo" + i] = i;
  }

  // Calling |transplant| transplants the object and then returns undefined.
  transplant(global);

  // Check the properties were copied over to the swapped object.
  for (let i = 0; i < count; i++) {
    assertEq(source["foo" + i], i);
  }
}

for (let global of globals) {
  for (let count of [0, 10, 30]) {
    testProperties(global, count);
  }
}
