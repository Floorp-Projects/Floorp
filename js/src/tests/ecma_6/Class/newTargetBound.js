function boundTarget(expected) {
    assertEq(new.target, expected);
}

let bound = boundTarget.bind(undefined);

const TEST_ITERATIONS = 550;

for (let i = 0; i < TEST_ITERATIONS; i++)
    bound(undefined);

for (let i = 0; i < TEST_ITERATIONS; i++)
    new bound(boundTarget);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
