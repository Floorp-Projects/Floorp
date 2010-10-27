var o = {
    g: function(a) {
	return a;
    }
};

function f() {
    var z;
    for (var i = 0; i < 10; ++i) {
	z = o.g(i);
	assertEq(z, i);
    }
}

f();
