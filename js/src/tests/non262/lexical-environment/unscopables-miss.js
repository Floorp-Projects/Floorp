// Trying to access a binding that doesn't exist due to @@unscopables
// is a ReferenceError.

with ({x: 1, [Symbol.unscopables]: {x: true}})
    assertThrowsInstanceOf(() => x, ReferenceError);

reportCompare(0, 0);
