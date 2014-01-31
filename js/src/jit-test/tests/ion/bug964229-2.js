function test1(re, test) {
    return re.test(test);
}

assertEq(true, test1(/undefined/, undefined));
assertEq(true, test1(/undefined/, undefined));

function test2(re, test) {
    return re.test(test);
}

assertEq(true, test2(/null/, null));
assertEq(true, test2(/null/, null));

function test3(re, test) {
    return re.test(test);
}

assertEq(true, test3(/0/, 0));
assertEq(true, test3(/0/, 0));

function test4(re, test) {
    return re.test(test);
}

assertEq(true, test4(/12.12/, 12.12));
assertEq(true, test4(/12.12/, 12.12));

function test5(re, test) {
    return re.test(test);
}

assertEq(true, test5(/true/, true));
assertEq(true, test5(/false/, false));
assertEq(true, test5(/true/, true));
assertEq(true, test5(/false/, false));

function test6(re, test) {
    return re.test(test);
}

assertEq(true, test6(/object/, {}));
assertEq(true, test6(/object/, {}));

assertEq(true, test1(/test/, "test"));
assertEq(true, test1(/test/, "test"));
assertEq(true, test1(/undefined/, undefined));
assertEq(true, test1(/undefined/, undefined));
assertEq(true, test1(/null/, null));
assertEq(true, test1(/null/, null));
assertEq(true, test1(/0.1/, 0.1));
assertEq(true, test1(/0.1/, 0.1));
assertEq(true, test1(/20000/, 20000));
assertEq(true, test1(/20000/, 20000));
assertEq(true, test1(/object/, {}));
assertEq(true, test1(/object/, {}));


