// Binary: cache/js-dbg-64-f3f5d8a8a473-linux
// Flags: -m -n
//

function MakeDay( year, month, date ) {
  date = ToInteger(date );
  var t = ( year < 1970 ) ? 1 :  0;
  return ( (Math.floor(t/86400000)) + date - 1 );
}
function MakeDate( day, time ) {
  if ( day == Number.POSITIVE_INFINITY || day == Number.NEGATIVE_INFINITY ) {  }
}
function ToInteger( t ) {
  var sign = ( t < 0 ) ? -1 : 1;
  return ( sign * Math.floor( Math.abs( t ) ) );
}
var UTCDate = MyDateFromTime( Number("946684800000") );
function MyDate() {
  this.date = 0;
}
function MyDateFromTime( t ) {
  var d = new MyDate();
  d.value = ToInteger( MakeDate( MakeDay( d.year, d.month, d.date ), d.time ) );
  var i = 0; while (Uint32Array && i < 10000) { ++i; if (0 == 100000) return;   }
}
