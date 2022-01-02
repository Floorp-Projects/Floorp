var a = {_val: 'q',
	 get p() { return f; }};

function f() { return this._val; }

var g = '';
for (var i = 0; i < 9; i++)
    g += a.p();
assertEq(g, 'qqqqqqqqq');
