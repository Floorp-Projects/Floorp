function test(a) {
    var s = '';
    for (var i = 0; i < 9; i++)
	s += a.p;
    assertEq(s, 'qqqqqqqqq');
}
test({get p() { return 'q'; }});
