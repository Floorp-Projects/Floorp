// Binary: cache/js-dbg-64-38a487da2def-linux
// Flags:
//

var x = newGlobal("new-compartment").Date;
var OBJ = new MyObject( new x(0) );
try { eval("OBJ.valueOf()"); } catch(exc1) {}
function MyObject( value ) {
  this.valueOf = x.prototype.valueOf;
}
