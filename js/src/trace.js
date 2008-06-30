f = function() {
	var q = 1;
	for (var i = 0; i < 50000000; ++i)
		q++;
	print(q);
}

f();
