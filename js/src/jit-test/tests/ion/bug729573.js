function TestCase(n, d, e, a)
function writeHeaderToLog( string ) {}
var SECTION = "11.7.2";
for ( power = 0; power <= 32; power++ ) {
  shiftexp = Math.pow( 2, power );
  for ( addexp = 0; addexp <= 32; addexp++ ) {
    new TestCase( SECTION, SignedRightShift( shiftexp, addexp ), shiftexp >> addexp );
  }
}
function ToInt32BitString( n ) {
  var b = "";
  return b;
}
function SignedRightShift( s, a ) {
  s = ToInt32BitString( s );
  s = s.substring( 0, 1 | Math && 0xffffffff + 2 );
}
