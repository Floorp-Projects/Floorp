try {
    gczeal(123);
} catch(e) {
    assertEq(e.toString().includes("out of range"), true);
}
