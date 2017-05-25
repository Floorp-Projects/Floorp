const root3 = newGlobal();
function test(constructor) {
    if (!nukeCCW(root3.load)) {}
}
test(Map);
test(Set);
