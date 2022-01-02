// |jit-test| skip-if: !('oomTest' in this)

oomTest(function() {
    eval("\
	function g() {\
	    \"use asm\";\
	    function f(d) {\
		d = +d;\
		print(.0 + d);\
	    }\
	}\
    ")
})
