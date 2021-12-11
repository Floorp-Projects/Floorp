var a = [1];
function f(x) {
    var round = Math.round;
    for (var i=0; i<20; i++) {
	a[0] = round(a[0]);
	if (x > 500)
	    a[0] = "a";
    }
}
for (var i=0; i<550; i++)
    f(i);
