// |jit-test| --fast-warmup; --no-threads

function dummy() { with ({}) {} }

function foo() {
    dummy();
    var x = [];
    var y = [];
    for (var i = 0; i < 10; i++) { }
    for (var i = 0; i < 100; i++) {
	var swap = x;
	x = y;
	y = swap;
    }
    return x;
}

foo();
