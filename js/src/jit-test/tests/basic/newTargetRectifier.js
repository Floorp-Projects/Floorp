function testBailoutNewTarget() {
    function Inner(ex, forceRectifier) {
        bailout();
        assertEq(new.target, ex);
    }

    for (let i = 0; i < 1100; i++)
        new Inner(Inner);
}

for (let i = 0; i < 15; i++)
    testBailoutNewTarget();
