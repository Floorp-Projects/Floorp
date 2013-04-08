function test() {
    var f;
    function gen() {
	f = function(){}
    }
    for (var i in gen()) {}
    arguments[arguments.length - 1];
}
test();
