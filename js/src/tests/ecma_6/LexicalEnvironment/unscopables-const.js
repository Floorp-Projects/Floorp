// @@unscopables prevents a property from having any effect on assigning to a
// const binding (which is an error).

const x = 1;
with ({x: 1, [Symbol.unscopables]: {x: true}})
    assertThrowsInstanceOf(() => {x = 2;}, TypeError);

reportCompare(0, 0);
