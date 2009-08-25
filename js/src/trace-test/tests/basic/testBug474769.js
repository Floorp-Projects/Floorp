delete b;
delete q;

for each (testBug474769_b in [1, 1, 1, 1.5, 1, 1]) {
    (function() { for each (let testBug474769_h in [0, 0, 1.4, ""]) {} })()
}
function testBug474769() {
    return testBug474769_b;
}
assertEq(testBug474769(), 1);
