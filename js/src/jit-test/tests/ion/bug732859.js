function TestCase(n, d, e, a)
function writeHeaderToLog( string ) {}
var SECTION = "15.1.2.4";
for ( var CHARCODE = 128; CHARCODE < 256; CHARCODE++ ) {
  new TestCase( SECTION, "%"+ToHexString(CHARCODE), escape(String.fromCharCode(CHARCODE)));
}
function ToHexString( n ) {
  var hex = new Array();
  hex[hex.length] = n % 16;
  var string ="";
  for ( var index = 0 ; index < hex.length ; index++ ) {
    switch ( hex[index] ) {
    case 10:
      string += "A";
    case 11:
    }
  }
}
