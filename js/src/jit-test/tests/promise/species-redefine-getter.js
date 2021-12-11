// Ensure the HadGetterSetterChange flag is set.
Object.defineProperty(Promise, "foo", {configurable: true, get: function() {}});
Object.defineProperty(Promise, "foo", {configurable: true, get: function() {}});

// Initialize PromiseLookup.
var p = new Promise(() => {});
for (let i = 0; i < 5; i++) {
    Promise.all([p]);
}

// Redefine the Promise[Symbol.species] getter without changing its attributes/shape.
let count = 0;
Object.defineProperty(Promise, Symbol.species,
                      {get: function() { count++; }, enumerable: false, configurable: true});

// Ensure PromiseLookup now deoptimizes and calls the getter.
for (let i = 0; i < 5; i++) {
    Promise.all([p]);
}
assertEq(count, 5);
