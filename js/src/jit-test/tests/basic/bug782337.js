gc();

function recur(n)
{
    if (n == 0) {
	gcslice();
	gc();
    } else {
	recur(n-1);
    }
    var obj = new Object();
}

gcslice(1);
recur(10);
