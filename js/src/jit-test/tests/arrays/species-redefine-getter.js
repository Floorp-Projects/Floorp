// Ensure the HadGetterSetterChange flag is set.
Object.defineProperty(Array, "foo", {configurable: true, get: function() {}});
Object.defineProperty(Array, "foo", {configurable: true, get: function() {}});

// Initialize ArraySpeciesLookup.
let a = [1, 2, 3];
for (let i = 0; i < 5; i++) {
    assertEq(a.slice().length, 3);
}

// Redefine the Array[Symbol.species] getter without changing its attributes/shape.
let count = 0;
Object.defineProperty(Array, Symbol.species,
                      {get: function() { count++; }, enumerable: false, configurable: true});

// Ensure ArraySpeciesLookup now deoptimizes and calls the getter.
for (let i = 0; i < 5; i++) {
    assertEq(a.slice().length, 3);
}
assertEq(count, 5);
