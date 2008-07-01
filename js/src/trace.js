f = function() {
	var q = 1;
	for (var i = 0; i < 50000000; ++i)
		q++;
	print(q);
}

var before = Date.now();
f();
var after = Date.now();
