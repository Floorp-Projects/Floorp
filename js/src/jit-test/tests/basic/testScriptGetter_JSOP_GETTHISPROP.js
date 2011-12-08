var a = {
    get p() { return 11; },
    test: function () {
	var s = 0;
	for (var i = 0; i < 9; i++)
	    s += this.p;
	assertEq(s, 99);
    }};
a.test();
