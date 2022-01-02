// Binary: cache/js-dbg-32-b5ae446888f5-linux
// Flags: -m -n -a
//

var Constr = function( ... property)  {};
Constr.prototype = [];
var c = new Constr();
c.push(5);
gc();
function enterFunc() {}
evaluate('enterFunc (c.length);');
