/* Don't assert. */
function f(x)
{
	if ("hi" == (x & 3)) {
		return 1;
	}
}
f(12);
