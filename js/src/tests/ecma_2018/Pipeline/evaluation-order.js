const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (evaluation order)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    eval(`
    // It tests evaluation order.
    // Considering the expression A |> B, A should be evaluated before B.
    let value = null;
    const result = (value = 10, "42") |> (value = 20, parseInt);
    assertEq(result, 42);
    assertEq(value, 20);
    `);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
