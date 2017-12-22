const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (eval)";

print(BUGNUMBER + ": " + summary);

const testvalue = 10;

if (hasPipeline()) {
    eval(`
    // Pipelined eval should be indirect eval call
    (() => {
        const testvalue = 20;
        assertEq("testvalue" |> eval, 10);
    })();
    `);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
