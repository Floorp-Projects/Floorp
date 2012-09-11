function TestCase(n, d, e, a)
function writeHeaderToLog( string ) {}
var SECTION = "15.1.2.5-2";
for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE += 16 ) {
  new TestCase( SECTION, unescape( "%" + (ToHexString(CHARCODE)).substring(0,1) )  );
}
function ToHexString( n ) {
  var hex = new Array();
  for ( var mag = 1; Math.pow(16,mag) <= n ; mag++ ) {  }
  for ( index = 0, mag -= 1; mag > 0; index++, mag-- ) {  }
  var string ="";
    switch ( hex[index] ) {
    case 10:
      string += "A";
  }
  return string;
}
