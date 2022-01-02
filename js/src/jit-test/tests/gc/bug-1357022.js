const root3 = newGlobal({newCompartment: true});
function test(constructor) {
    if (!nukeCCW(root3.load)) {}
}
test(Map);
test(Set);
