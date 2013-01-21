// Binary: cache/js-dbg-64-fcca99426576-linux
// Flags: -m -n -a
//

function printStatus (msg) {}
F = function () {};
F.prototype = new Int32Array(1);
o = new F();
function f2(o){
	with(this)
	for(var x in o)
	printStatus(o[x]);
}
f2([]);
