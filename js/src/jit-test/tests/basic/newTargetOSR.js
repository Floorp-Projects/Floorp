function testOSRNewTarget(expected) {
    for (let i = 0; i < 1100; i++)
        assertEq(new.target, expected);
}

new testOSRNewTarget(testOSRNewTarget);
