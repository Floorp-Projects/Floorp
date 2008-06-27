f = function() {
	var q = 1;
	for (var i = 0; i < 50; ++i)
		q &= 0x88888888;
}

f();
