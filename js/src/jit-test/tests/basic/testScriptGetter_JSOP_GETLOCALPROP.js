function test() {
    var a = {get p() { return 'q'; }};
    var s = '';
    for (var i = 0; i < 9; i++)
	s += a.p;
    assertEq(s, 'qqqqqqqqq');
}
test();
