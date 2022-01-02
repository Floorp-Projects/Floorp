// |jit-test| --no-threads; --baseline-warmup-threshold=1; --ion-warmup-threshold=0

var input = ["", 0, "", "", "", "", "", "", "", "", "", "", "", "", {}, {}, ""];

for (var i = 0; i < 10; i++) {
    function sum_indexing(x,i) {
	if (i == x.length) {
	    return 0;
	} else {
	    return x[i] + sum_indexing(x, i+1);
	}
    }
    sum_indexing(input, 0);
}
