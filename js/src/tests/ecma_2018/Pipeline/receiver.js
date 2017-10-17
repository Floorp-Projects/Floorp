const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (receiver)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    // Receiver
    eval(`
    const receiver = {
        foo: function (value, extra) {
            assertEq(value, 10);
            assertEq(extra, undefined);
            assertEq(arguments.length, 1);
            assertEq(arguments[0], 10)
            assertEq(this, receiver);
        },
    };

    10 |> receiver.foo;
    `);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
