// Binary: cache/js-dbg-64-4de07a341aab-linux
// Flags: -m -n -a
//
this.__proto__ = [];
var msPerDay =   86400000;
for ( var time = 0, year = 1969; year >= 0; year-- ) {
  time -= TimeInYear(year);
}
function DaysInYear( y ) {}
function TimeInYear( y ) {
  return ( DaysInYear(y) * msPerDay );
}
