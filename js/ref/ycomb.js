// The Y combinator, applied to the factorial function

function factwrap(proc) {
    function factproc(n) {
	if (n <= 1) return 1
	return n * proc(n-1)
    }
    return factproc
}

function Y(outer) {
    function inner(proc) {
	function apply(arg) {
	    return proc(proc)(arg)
	}
	return outer(apply)
    }
    return inner(inner)
}

print("5! is " + Y(factwrap)(5))
