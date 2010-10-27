function testEliminatedGuardWithinAnchor() {
    for (let i = 0; i < 5; ++i) { i / (i * i); }
    return "ok";
}

assertEq(testEliminatedGuardWithinAnchor(), "ok");

if (HAVE_TM) {
    checkStats({
        sideExitIntoInterpreter: (jitstats.archIsARM ? 1 : 3)
    });
}
