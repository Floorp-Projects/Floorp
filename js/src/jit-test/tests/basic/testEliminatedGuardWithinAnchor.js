function testEliminatedGuardWithinAnchor() {
    for (let i = 0; i < 5; ++i) { i / (i * i); }
    return "ok";
}

assertEq(testEliminatedGuardWithinAnchor(), "ok");
