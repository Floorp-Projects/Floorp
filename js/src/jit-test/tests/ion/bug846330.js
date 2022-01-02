
function outer(code) {
    function inner() {
	eval(code);
    }
    inner();
}
outer("1");
outer("assertEq(typeof arguments, 'object');");
