//test no multitrees assert
function testBug463490() {
    function f(a, b, d) {
        for (var i = 0; i < 10; i++) {
            if (d)
                b /= 2;
        }
        return a + b;
    }
    //integer stable loop
    f(2, 2, false);
    //double stable loop
    f(3, 4.5, false);
    //integer unstable branch
    f(2, 2, true);
    return true;
};
assertEq(testBug463490(), true);
