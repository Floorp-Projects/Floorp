// |jit-test| need-for-each

function testUndemoteLateGlobalSlots() {
    for each (aaa in ["", "", 0/0, ""]) {
        ++aaa;
        for each (bbb in [0, "", aaa, "", 0, "", 0/0]) {
        }
    }
    delete aaa;
    delete bbb;
    return "ok";
}
assertEq(testUndemoteLateGlobalSlots(), "ok");
