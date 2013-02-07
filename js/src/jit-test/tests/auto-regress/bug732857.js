// Binary: cache/js-dbg-32-1fd6c40d3852-linux
// Flags: --ion-eager
//

function TestCase(n, d, e, a) {};
  this.__proto__ = [];
var msPerDay =   86400000;
var TIME_1900  = -2208988800000;
function TimeFromYear( y ) {
  return ( msPerDay * DayFromYear(y) );
}
function DayFromYear( y ) {
  return ( 365*(y-1970) +
           Math.floor((y-1601)/400) );
}
function YearFromTime( t ) {
  var sign = ( t < 0 ) ? -1 : 1;
  var year = ( sign < 0 ) ? 1969 : 1970;
  return ( year );
}
var SECTION = "15.9.5.10";
addTestCase( TIME_1900 );
function addTestCase( t ) {
  var start = TimeFromYear(YearFromTime(t));
  var stop  = TimeFromYear(YearFromTime(t) + 1);
  for (var d = start; d < stop; d += msPerDay) {
    new TestCase( SECTION,
                  (new Date( SECTION   ? "Failed" : prototype +=  "'abc'.search(new RegExp('^'))") ).getDate() );
  }
}
