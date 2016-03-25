// Accessing an uninitialized variable due to @@unscopables is still a ReferenceError.

with ({x: 1, [Symbol.unscopables]: {x: true}})
    assertThrowsInstanceOf(() => x, ReferenceError);

let x;

reportCompare(0, 0);

