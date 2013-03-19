//|jit-test| error: InternalError

var LIMIT = 65535;
for ( var i = 0, args = "" ; i < LIMIT; i++ ) {
  args += String(i) + ( i+1 < LIMIT ? "," : "" );
}
var LENGTH = eval( "GetLength("+ args +")" );
