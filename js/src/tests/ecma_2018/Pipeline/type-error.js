const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (type error)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    // Type error
    eval(`
    assertThrowsInstanceOf(() => 10 |> 10, TypeError);
    assertThrowsInstanceOf(() => 10 |> "foo", TypeError);
    assertThrowsInstanceOf(() => 10 |> null, TypeError);
    assertThrowsInstanceOf(() => 10 |> undefined, TypeError);
    assertThrowsInstanceOf(() => 10 |> true, TypeError);
    assertThrowsInstanceOf(() => 10 |> { a: 1 }, TypeError);
    assertThrowsInstanceOf(() => 10 |> [ 0, 1 ], TypeError);
    assertThrowsInstanceOf(() => 10 |> /regexp/, TypeError);
    assertThrowsInstanceOf(() => 10 |> Symbol(), TypeError);
    `);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
